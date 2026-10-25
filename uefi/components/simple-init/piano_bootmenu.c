// SPDX-License-Identifier: LGPL-3.0-or-later
// Product menu presentation; OS/reset actions stay with the resident core.
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "confd.h"
#include "gui.h"
#include "gui/activity.h"
#include "gui/filepicker.h"
#include "gui/msgbox.h"
#include "piano_bootmenu.h"

static int compare_entries(const void *left, const void *right) {
    const char *a = *(const char *const *)left;
    const char *b = *(const char *const *)right;
    int64_t a_order = confd_get_integer_dict("boot.configs", a, "order", 1000);
    int64_t b_order = confd_get_integer_dict("boot.configs", b, "order", 1000);
    if (a_order != b_order) return a_order < b_order ? -1 : 1;
    return strcmp(a, b);
}

void piano_bootmenu_sort(char **names) {
    size_t count = 0;
    if (!names) return;
    while (names[count]) ++count;
    qsort(names, count, sizeof(*names), compare_entries);
}

static char selected_file[PATH_MAX];
static bool boot_file_pending;

static void start_selected_file(void *unused) {
    (void)unused;
    // The picker queues its back task after its callback. Defer one more
    // async slice so the new confirmation isn't mistaken for the picker.
    if (boot_file_pending) {
        boot_file_pending = false;
        lv_async_call(start_selected_file, NULL);
        return;
    }
    guiact_start_activity_by_name("uefi-app-start", selected_file);
}

static bool select_boot_file(bool ok, const char **paths, uint16_t count, void *unused) {
    (void)unused;
    bool selected = ok && count == 1 && paths && paths[0];
    if (selected) {
        size_t bytes = strlen(paths[0]);
        if (bytes < 4 || bytes >= sizeof(selected_file) || strcasecmp(paths[0] + bytes - 4, ".efi")) {
            msgbox_alert("请选择 EFI 启动文件（.efi）。");
            free((void *)paths);
            return true;
        }
        memcpy(selected_file, paths[0], bytes + 1);
    }
    free((void *)paths);
    // Start on the next GUI slice, after the picker has scheduled its close.
    if (selected) { boot_file_pending = true; lv_async_call(start_selected_file, NULL); }
    return false;
}

static void pick_boot_file(void *unused) {
    (void)unused;
    struct filepicker *picker = filepicker_create(select_boot_file, "从文件启动");
    filepicker_set_max_item(picker, 1);
}

void piano_bootmenu_enter(void *data) {
    char *activity = data;
    gui_screen_init();
    gui_draw();
    if (activity && *activity) {
        if (!strcmp(activity, "boot-file")) lv_async_call(pick_boot_file, NULL);
        else guiact_start_activity_by_name(activity, NULL);
    }
    free(activity);
}
