#include "clock_service.h"
#include "vfs.h"

#include <stdio.h>
#include <stdlib.h>

static time_t g_saved;

static time_t read_saved(void) {
    vfs_file_t* file = NULL;
    char buf[32];
    ssize_t n;
    char* end = NULL;
    long value;

    if (vfs_open(CLOCK_SERVICE_PATH, VFS_MODE_READ, &file) != 0) {
        return 0;
    }
    n = vfs_read(file, buf, sizeof(buf) - 1);
    vfs_close(file);
    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';
    value = strtol(buf, &end, 10);
    if (end == buf || value <= (long)OS_CLOCK_SET_AFTER) {
        return 0;
    }
    return (time_t)value;
}

static int write_saved(time_t unix_time) {
    vfs_file_t* file = NULL;
    char buf[32];
    int n;
    ssize_t wrote;

    n = snprintf(buf, sizeof(buf), "%ld\n", (long)unix_time);
    if (n <= 0 || (size_t)n >= sizeof(buf)) {
        return -1;
    }
    if (vfs_open(CLOCK_SERVICE_PATH, VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file) !=
        0) {
        return -1;
    }
    wrote = vfs_write(file, buf, (size_t)n);
    vfs_sync(file);
    vfs_close(file);
    if (wrote != n) {
        return -1;
    }
    g_saved = unix_time;
    return 0;
}

int clock_service_start(time_t rtc_now, time_t compiled_unix) {
    time_t saved = read_saved();
    time_t chosen = 0;

    if (saved > (time_t)OS_CLOCK_SET_AFTER) {
        chosen = saved;
        g_saved = saved;
    } else if (rtc_now > (time_t)OS_CLOCK_SET_AFTER) {
        chosen = rtc_now;
    } else if (compiled_unix > (time_t)OS_CLOCK_SET_AFTER) {
        chosen = compiled_unix;
    }
    if (chosen == 0) {
        return -1;
    }
    if (saved == 0 && write_saved(chosen) != 0) {
        g_saved = chosen;
    }
    if (os_clock_stamp(rtc_now, chosen) != 0) {
        os_clock_set(chosen);
    }
    return 0;
}

int clock_service_shift(int32_t delta_s) {
    if (os_clock_shift(delta_s) != 0) {
        return -1;
    }
    return write_saved(os_clock_now());
}

int clock_service_set(time_t unix_time) {
    if (os_clock_set(unix_time) != 0) {
        return -1;
    }
    return write_saved(unix_time);
}

int clock_service_checkpoint(void) {
    time_t now = os_clock_now();
    if (g_saved != 0 && now >= g_saved && (now - g_saved) < 60) {
        return 0;
    }
    return write_saved(now);
}
