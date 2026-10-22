// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 slwyts <slwyts@foxmail.com>
 *
 * Native Piano Goodix control client. The application interface follows the
 * factory libgf_ca/QSEECom calls; transport is Qualcomm's public MinkIPC API.
 * Device information and sensor initialization are implemented. Enrollment
 * and authentication still require the Goodix command protocol and libfprint.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "MinkCom.h"

#define FIRMWARE "/usr/lib/firmware/gfpia.elf"
#define FIRMWARE_LIMIT (32U * 1024U * 1024U)
#define LOADER_UID 122
#define PAYLOAD_SIZE 640
#define SHARED_SIZE 4096
#define MODEL_BYTES 0x124ff4
#define MODEL_CHUNK_BYTES 0x7cf9c
#define MODEL_PACKET_BYTES (MODEL_CHUNK_BYTES + 0x78)
static volatile sig_atomic_t interrupted;

static void stop_watching(int signal_number)
{
	(void)signal_number;
	interrupted = 1;
}

struct client {
	Object root, env, loader, app, memory;
	void *shared;
	size_t shared_size;
	uint32_t architecture;
	uint32_t sequence;
	int controller;
	int sensor_started;
};

struct goodix_request {
	uint64_t buffer;
	uint32_t sequence;
	uint32_t route;
	uint64_t milliseconds;
	uint32_t log_level;
	uint32_t reserved;
	uint32_t command;
	uint32_t payload_size;
	uint8_t padding[88];
};
_Static_assert(sizeof(struct goodix_request) == 128, "Goodix request ABI");

struct sensor_geometry {
	uint32_t rows, columns, nav_rows, nav_columns;
	uint32_t chip_type, sensor_type;
	uint32_t model_required;
	uint8_t anti_spoof_required;
	uint8_t anti_spoof_variant;
	int models_loaded;
};

static int sensor_command(struct client *client, uint32_t command, size_t size);

static int invoke(Object object, ObjectOp op, ObjectArg *args, ObjectCounts counts)
{
	int32_t result = Object_invoke(object, op, args, counts);

	if (result) {
		fprintf(stderr, "QTEE operation %u failed: %d\n", (unsigned int)op,
			result);
		return -EIO;
	}
	return 0;
}

static int close_client(struct client *client)
{
	int result = 0;

	if (client->sensor_started && client->shared) {
		memset(client->shared, 0, client->shared_size);
		result = sensor_command(client, 1002, 0x6c);
	}
	if (!Object_isNull(client->app)) {
		int shutdown = invoke(client->app, 2, NULL, ObjectCounts_pack(0, 0, 0, 0));
		if (!result)
			result = shutdown;
	}
	if (client->controller >= 0)
		close(client->controller);
	Object_RELEASE_IF(client->memory);
	client->memory = Object_NULL;
	Object_RELEASE_IF(client->app);
	Object_RELEASE_IF(client->loader);
	Object_RELEASE_IF(client->env);
	Object_RELEASE_IF(client->root);
	return result;
}

static int open_controller(void)
{
	glob_t matches = {0};
	int fd = -ENODEV;

	if (glob("/sys/class/uio/uio*/name", 0, NULL, &matches))
		return -ENODEV;
	for (size_t i = 0; i < matches.gl_pathc; i++) {
		char name[64], path[128];
		FILE *stream = fopen(matches.gl_pathv[i], "r");
		unsigned int index;

		if (!stream)
			continue;
		char *read = fgets(name, sizeof(name), stream);
		fclose(stream);
		if (!read || strcmp(name, "piano-goodix\n"))
			continue;
		if (sscanf(matches.gl_pathv[i], "/sys/class/uio/uio%u/name", &index) != 1)
			continue;
		snprintf(path, sizeof(path), "/dev/uio%u", index);
		fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			fd = -errno;
		break;
	}
	globfree(&matches);
	return fd;
}

static int load_application(struct client *client)
{
	struct stat st;
	Object image = Object_NULL;
	void *address = NULL;
	size_t capacity = 0;
	const char name[] = "gfpia";
	int fd = open(FIRMWARE, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	int result = -EINVAL;

	if (fd < 0)
		return -errno;
	if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 64 ||
	    st.st_size > FIRMWARE_LIMIT)
		goto done;
	if (MinkCom_getMemoryObject(client->root, st.st_size, &image) ||
	    MinkCom_getMemoryObjectInfo(image, &address, &capacity) ||
	    capacity < (size_t)st.st_size) {
		result = -ENOMEM;
		goto done;
	}
	for (size_t offset = 0; offset < (size_t)st.st_size;) {
		ssize_t count = read(fd, (uint8_t *)address + offset, st.st_size - offset);

		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0) {
			result = -EIO;
			goto done;
		}
		offset += count;
	}
	if (memcmp(address, "\177ELF", 4))
		goto done;
	ObjectArg load[] = {
		{ .bi = {name, sizeof(name) - 1} },
		{ .o = image },
		{ .o = Object_NULL },
	};
	result = invoke(client->loader, 0, load, ObjectCounts_pack(1, 0, 1, 1));
	if (result)
		goto done;
	client->app = load[2].o;
	/* The signed container can be ELF32 while the command interface is
	 * 64-bit. Ask the loader instead of inferring this from the file.
	 */
	ObjectArg lookup[] = {
		{ .bi = {name, sizeof(name) - 1} },
		{ .b = {&client->architecture, sizeof(client->architecture)} },
		{ .o = Object_NULL },
	};
	result = invoke(client->loader, 2, lookup, ObjectCounts_pack(1, 1, 0, 1));
	Object_RELEASE_IF(lookup[2].o);
	if (!result && client->architecture != 1 && client->architecture != 2)
		result = -EOPNOTSUPP;
done:
	Object_RELEASE_IF(image);
	close(fd);
	return result;
}

static int open_client(struct client *client)
{
	uint32_t uid = LOADER_UID;
	int result;

	memset(client, 0, sizeof(*client));
	client->controller = -1;
	if (MinkCom_getRootEnvObject(&client->root) ||
	    MinkCom_getClientEnvObject(client->root, &client->env))
		return -EIO;
	ObjectArg args[] = {
		{ .bi = {&uid, sizeof(uid)} },
		{ .o = Object_NULL },
	};
	result = invoke(client->env, 0, args, ObjectCounts_pack(1, 0, 0, 1));
	if (result)
		return result;
	client->loader = args[1].o;
	result = load_application(client);
	if (result)
		return result;
	/* Keep the factory ordering: load signed TA, then power/reset sensor. */
	client->controller = open_controller();
	if (client->controller < 0)
		return client->controller;
	if (MinkCom_getMemoryObject(client->root, SHARED_SIZE, &client->memory) ||
	    MinkCom_getMemoryObjectInfo(client->memory, &client->shared,
				       &client->shared_size))
		return -ENOMEM;
	return client->shared_size >= SHARED_SIZE ? 0 : -EMSGSIZE;
}

static int sensor_command(struct client *client, uint32_t command, size_t size)
{
	struct goodix_request request = {0};
	int32_t response[16] = {0};
	uint32_t offsets[4] = {0};
	uint32_t is_64_bit = client->architecture == 2;
	time_t now = time(NULL);
	struct tm local;
	int result;

	if (size < 0x6c || size > client->shared_size)
		return -EMSGSIZE;
	memcpy((uint8_t *)client->shared + 0x44, &client->sequence,
	       sizeof(client->sequence));
	localtime_r(&now, &local);
	/* The Milan HAL supplies this bounded timestamp in its common header.
	 * It is descriptive metadata, not an authentication token.
	 */
	strftime((char *)client->shared + 0x4c, 0x20, "%Y-%m-%d %H:%M:%S", &local);
	request.sequence = ++client->sequence;
	request.route = 1;
	request.command = command;
	request.payload_size = size;
	request.milliseconds = (uint64_t)(local.tm_hour * 3600 +
		local.tm_min * 60 + local.tm_sec) * 1000;
	ObjectArg args[] = {
		{ .bi = {&request, sizeof(request)} },
		{ .bi = {response, sizeof(response)} },
		{ .bi = {offsets, sizeof(offsets)} },
		{ .bi = {&is_64_bit, sizeof(is_64_bit)} },
		{ .b = {&request, sizeof(request)} },
		{ .b = {response, sizeof(response)} },
		{ .o = client->memory },
		{ .o = Object_NULL },
		{ .o = Object_NULL },
		{ .o = Object_NULL },
	};
	result = invoke(client->app, 0, args, ObjectCounts_pack(4, 2, 4, 0));
	if (result)
		return result;
	if (args[5].b.size < 2 * sizeof(int32_t))
		return -EMSGSIZE;
	if (response[0] || response[1]) {
		fprintf(stderr, "Goodix command %u: application=%d buffer=%d\n",
			command, response[0], response[1]);
		return -EREMOTEIO;
	}
	return 0;
}

static int device_info(struct client *client, uint32_t *sensor_type)
{
	for (uint32_t candidate = 4; candidate; candidate--) {
		int result;

		if (candidate != 4) {
			close(client->controller);
			usleep(5000);
			client->controller = open_controller();
			if (client->controller < 0)
				return client->controller;
		}
		memset(client->shared, 0, client->shared_size);
		((uint8_t *)client->shared)[0x278] = candidate;
		result = sensor_command(client, 1000, PAYLOAD_SIZE);
		if (result && result != -EREMOTEIO)
			return result;
		if (result)
			continue;
		memcpy(sensor_type, (uint8_t *)client->shared + 0x74,
		       sizeof(*sensor_type));
		return *sensor_type ? 0 : -ENODEV;
	}
	return -ENODEV;
}

static int read_model(const char *name, uint8_t *destination, size_t size)
{
	char path[256];
	struct stat st;
	size_t offset = 0;
	int result = 0;

	snprintf(path, sizeof(path), "/usr/lib/firmware/%s", name);
	int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return -errno;
	if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size != (off_t)size) {
		result = -EMSGSIZE;
		goto done;
	}
	while (offset < size) {
		ssize_t count = read(fd, destination + offset, size - offset);
		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0) {
			result = -EIO;
			break;
		}
		offset += count;
	}
done:
	close(fd);
	return result;
}

static int send_model(struct client *client, const uint8_t *model)
{
	void *shared;
	size_t capacity;

	/* The factory sends three bounded pieces. Only the current piece is
	 * shared with QTEE; the assembled model remains ordinary CPU memory.
	 */
	Object_RELEASE_IF(client->memory);
	client->memory = Object_NULL;
	client->shared = NULL;
	client->shared_size = 0;
	if (MinkCom_getMemoryObject(client->root, MODEL_PACKET_BYTES, &client->memory) ||
	    MinkCom_getMemoryObjectInfo(client->memory, &shared, &capacity) ||
	    capacity < MODEL_PACKET_BYTES)
		return -ENOMEM;
	client->shared = shared;
	client->shared_size = capacity;
	for (size_t offset = 0, part = 0; offset < MODEL_BYTES;
	     offset += MODEL_CHUNK_BYTES, part++) {
		uint8_t *payload = shared;
		size_t bytes = MODEL_BYTES - offset;
		uint32_t total = MODEL_BYTES, command = 1111;
		int result;

		if (bytes > MODEL_CHUNK_BYTES)
			bytes = MODEL_CHUNK_BYTES;
		memset(payload, 0, capacity);
		memcpy(payload + 0x6c, model + offset, bytes);
		payload[0x7d008] = part;
		memcpy(payload + 0x7d00c, &total, sizeof(total));
		memcpy(payload + 0x7d010, &command, sizeof(command));
		result = sensor_command(client, 1113, MODEL_PACKET_BYTES);
		if (result)
			return result;
	}
	return 0;
}

static int load_models(struct client *client, const struct sensor_geometry *geometry)
{
	int result = 0;
	uint8_t *model;

	if (!geometry->model_required && !geometry->anti_spoof_required)
		return 0;
	/* Fixed model selection recovered for this actual chip. Do not use a
	 * similarly named model from another Goodix sensor.
	 */
	if (geometry->chip_type != 49)
		return -EOPNOTSUPP;
	model = calloc(1, MODEL_BYTES);
	if (!model)
		return -ENOMEM;
	if (geometry->anti_spoof_required) {
		uint32_t size = 308544;
		const char *name = geometry->anti_spoof_variant ?
			"piano-goodix-cap-t.bin" : "piano-goodix-cap-s.bin";

		result = read_model(name, model + 0x70, size);
		if (result)
			goto done;
		memcpy(model + 0x1248b8, &size, sizeof(size));
		result = send_model(client, model);
		if (result)
			goto done;
	}
	if (geometry->model_required) {
		uint32_t elements = 74530;

		memset(model, 0, MODEL_BYTES);
		model[0x6c] = 1;
		result = read_model("piano-goodix-dubai-u4.bin", model + 0x70,
				    elements * sizeof(uint32_t));
		if (result)
			goto done;
		memcpy(model + 0x124ff0, &elements, sizeof(elements));
		result = send_model(client, model);
	}
done:
	free(model);
	return result;
}

static int initialize_sensor(struct client *client, struct sensor_geometry *geometry)
{
	int result;
	uint32_t chip_type, sensor_type;
	uint8_t *payload = client->shared;

	if (geometry->sensor_type != 5)
		return -EOPNOTSUPP;
	memcpy(&geometry->chip_type, payload + 0x70, sizeof(uint32_t));
	memcpy(&geometry->model_required, payload + 0x1ec, sizeof(uint32_t));
	geometry->anti_spoof_required = payload[0x20d];
	geometry->anti_spoof_variant = payload[0x212];
	/* Milan initialization, recovered from the factory HAL's sensor-type-5
	 * function table. Empty OTP cache means read calibration from the real
	 * sensor. No Android storage, user template or enrollment command is used.
	 */
	memset(payload, 0, client->shared_size);
	result = sensor_command(client, 1001, 0x168);
	if (result)
		return result;
	client->sensor_started = 1;
	memcpy(&geometry->rows, payload + 0x70, sizeof(uint32_t));
	memcpy(&geometry->columns, payload + 0x74, sizeof(uint32_t));
	memcpy(&geometry->nav_rows, payload + 0x80, sizeof(uint32_t));
	memcpy(&geometry->nav_columns, payload + 0x84, sizeof(uint32_t));
	memcpy(&chip_type, payload + 0x15c, sizeof(uint32_t));
	memcpy(&sensor_type, payload + 0x160, sizeof(uint32_t));
	/* As in the factory HAL, zero here means retain the device-info value. */
	if (chip_type)
		geometry->chip_type = chip_type;
	if (sensor_type)
		geometry->sensor_type = sensor_type;
	if (!geometry->rows || !geometry->columns ||
	    geometry->rows > 4096 || geometry->columns > 4096)
		return -EPROTO;
	result = load_models(client, geometry);
	if (result)
		return result;
	geometry->models_loaded = 1;
	memset(client->shared, 0, client->shared_size);
	/* GF_CMD_INIT_FINISH: no debug dumping, matching the stock default. */
	result = sensor_command(client, 1005, 0x74);
	if (result)
		return result;
	memset(client->shared, 0, client->shared_size);
	/* Factory default interaction policy, without Android debug overrides. */
	return sensor_command(client, 1109, 0xc0);
}

static int watch_events(struct client *client, unsigned int seconds)
{
	struct timespec start, now;
	struct pollfd event = { .fd = client->controller, .events = POLLIN };
	uint8_t *payload = client->shared;
	int result;

	if (client->shared_size < 0xd134)
		return -EMSGSIZE;
	memset(payload, 0, client->shared_size);
	result = sensor_command(client, 1009, 0x70); /* idle / finger detection */
	if (result)
		return result;
	signal(SIGINT, stop_watching);
	signal(SIGTERM, stop_watching);
	clock_gettime(CLOCK_MONOTONIC, &start);
	fprintf(stderr, "Sensor ready; watching interrupts for %u seconds.\n", seconds);
	while (!interrupted) {
		uint32_t counter, mask, enable = 1, one = 1, maximum = 512;
		uint32_t flags = 0x02000000;
		clock_gettime(CLOCK_MONOTONIC, &now);
		int64_t remaining = (int64_t)seconds * 1000 -
			((now.tv_sec - start.tv_sec) * 1000 +
			 (now.tv_nsec - start.tv_nsec) / 1000000);
		if (remaining <= 0)
			break;
		int ready = poll(&event, 1, remaining > 500 ? 500 : remaining);
		if (ready < 0 && errno == EINTR)
			continue;
		if (ready < 0)
			return -errno;
		if (!ready)
			continue;
		if (!(event.revents & POLLIN))
			return -EIO;
		if (read(client->controller, &counter, sizeof(counter)) != sizeof(counter))
			return -EIO;
		memset(payload, 0, client->shared_size);
		memcpy(payload + 0x98, &one, sizeof(one));
		memcpy(payload + 0xa8, &maximum, sizeof(maximum));
		memcpy(payload + 0x2c5, &flags, sizeof(flags));
		/* Query/acknowledge the sensor before allowing its next IRQ.
		 * No image or template data is saved or printed.
		 */
		result = sensor_command(client, 1016, 0xd134);
		if (result)
			return result;
		memcpy(&mask, payload + 0x6c, sizeof(mask));
		printf("{\"irq_count\":%u,\"sensor_events\":%u}\n", counter, mask);
		fflush(stdout);
		if (write(client->controller, &enable, sizeof(enable)) != sizeof(enable))
			return -EIO;
	}
	return 0;
}

int main(int argc, char **argv)
{
	struct client client = { .controller = -1 };
	struct sensor_geometry geometry = {0};
	uint32_t sensor_type = 0;
	int watch = argc >= 2 && !strcmp(argv[1], "watch");
	int initialize = argc >= 2 && (!strcmp(argv[1], "initialize") || watch);
	unsigned int seconds = 30;
	int result;

	if (argc == 1 || (argc == 2 && !strcmp(argv[1], "--help"))) {
		puts("Usage: piano-fingerprint-ctl info|initialize|watch [seconds]\n"
		     "Query the real Goodix sensor through UIO and QTEE.\n"
		     "initialize also reads the sensor's geometry and calibration.\n"
		     "watch reports sensor event flags for 1-120 seconds (default 30).\n"
		     "Enrollment and authentication are not implemented yet.");
		return 0;
	}
	if ((argc != 2 && !(watch && argc == 3)) ||
	    (!initialize && strcmp(argv[1], "info"))) {
		fputs("Use --help for supported operations.\n", stderr);
		return 2;
	}
	if (watch && argc == 3) {
		char *end;
		unsigned long value = strtoul(argv[2], &end, 10);
		if (!*argv[2] || *end || value < 1 || value > 120) {
			fputs("Watch duration must be between 1 and 120 seconds.\n", stderr);
			return 2;
		}
		seconds = value;
	}
	result = open_client(&client);
	if (!result)
		result = device_info(&client, &sensor_type);
	if (!result && initialize) {
		geometry.sensor_type = sensor_type;
		result = initialize_sensor(&client, &geometry);
	}
	if (!result && watch)
		result = watch_events(&client, seconds);
	int closed = close_client(&client);
	if (!result)
		result = closed;
	if (!result && initialize)
		printf("{\"sensor\":\"Goodix\",\"sensor_type\":%u,"
		       "\"chip_type\":%u,\"rows\":%u,\"columns\":%u,"
		       "\"nav_rows\":%u,\"nav_columns\":%u,"
		       "\"model_required\":%u,\"anti_spoof_required\":%u,"
		       "\"models_loaded\":%s,\"sensor_initialized\":true}\n",
		       geometry.sensor_type, geometry.chip_type, geometry.rows,
		       geometry.columns, geometry.nav_rows, geometry.nav_columns,
		       geometry.model_required, geometry.anti_spoof_required,
		       geometry.models_loaded ? "true" : "false");
	else if (!result)
		printf("{\"sensor\":\"Goodix\",\"sensor_type\":%u,"
		       "\"command_address_bits\":%u,\"device_info_ok\":true}\n",
		       sensor_type, client.architecture == 2 ? 64 : 32);
	if (result)
		fprintf(stderr, "Fingerprint operation failed: %s (%d)\n",
			strerror(-result), result);
	return result ? 1 : 0;
}
