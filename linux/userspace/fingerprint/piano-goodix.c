// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 slwyts <slwyts@foxmail.com>
 *
 * Native Piano Goodix control client. The application interface follows the
 * factory libgf_ca/QSEECom calls; transport is Qualcomm's public MinkIPC API.
 * This currently queries device information, not enrollment or authentication.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
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

struct client {
	Object root, env, loader, app, memory;
	void *shared;
	size_t shared_size;
	uint32_t architecture;
	uint32_t sequence;
	int controller;
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

	if (!Object_isNull(client->app))
		result = invoke(client->app, 2, NULL, ObjectCounts_pack(0, 0, 0, 0));
	if (client->controller >= 0)
		close(client->controller);
	Object_RELEASE_IF(client->memory);
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

static int device_info(struct client *client, uint32_t *sensor_type)
{
	for (uint32_t candidate = 4; candidate; candidate--) {
		struct goodix_request request = {0};
		int32_t response[16] = {0};
		uint32_t offsets[4] = {0};
		uint32_t is_64_bit = client->architecture == 2;
		time_t now = time(NULL);
		struct tm local;
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
		memcpy((uint8_t *)client->shared + 0x44, &client->sequence,
		       sizeof(client->sequence));
		localtime_r(&now, &local);
		request.sequence = ++client->sequence;
		request.route = 1;
		request.command = 1000; /* GF_CMD_GET_DEV_INFO */
		request.payload_size = PAYLOAD_SIZE;
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
		if (response[0] || response[1])
			continue;
		memcpy(sensor_type, (uint8_t *)client->shared + 0x74,
		       sizeof(*sensor_type));
		return *sensor_type ? 0 : -ENODEV;
	}
	return -ENODEV;
}

int main(int argc, char **argv)
{
	struct client client = { .controller = -1 };
	uint32_t sensor_type = 0;
	int result;

	if (argc == 1 || (argc == 2 && !strcmp(argv[1], "--help"))) {
		puts("Usage: piano-fingerprint-ctl info\n"
		     "Query the real Goodix sensor through UIO and QTEE.\n"
		     "Enrollment and authentication are not implemented yet.");
		return 0;
	}
	if (argc != 2 || strcmp(argv[1], "info")) {
		fputs("Use --help for supported operations.\n", stderr);
		return 2;
	}
	result = open_client(&client);
	if (!result)
		result = device_info(&client, &sensor_type);
	int closed = close_client(&client);
	if (!result)
		result = closed;
	if (!result)
		printf("{\"sensor\":\"Goodix\",\"sensor_type\":%u,"
		       "\"command_address_bits\":%u,\"device_info_ok\":true}\n",
		       sensor_type, client.architecture == 2 ? 64 : 32);
	if (result)
		fprintf(stderr, "Fingerprint device query failed: %s (%d)\n",
			strerror(-result), result);
	return result ? 1 : 0;
}
