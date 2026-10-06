#include "os_clock.h"

#include <stdio.h>

#if defined(ARDUBOT_TARGET_ESP32)
#include <sys/time.h>
#endif

#if !defined(ARDUBOT_TARGET_ESP32)
static int32_t g_adjust_s;
#endif

time_t os_clock_stamp(time_t rtc_now, time_t compiled_unix) {
    if (rtc_now > (time_t)OS_CLOCK_SET_AFTER) {
        return 0;
    }
    if (compiled_unix > (time_t)OS_CLOCK_SET_AFTER) {
        return compiled_unix;
    }
    return 0;
}

int os_clock_tz_string(int offset_min, char* buf, size_t cap) {
    int west;
    int abs_min;
    int hours;
    int mins;

    if (!buf || cap < 12) {
        return -1;
    }
    west = -offset_min;
    if (west == 0) {
        snprintf(buf, cap, "UTC0");
        return 0;
    }
    abs_min = west < 0 ? -west : west;
    hours = abs_min / 60;
    mins = abs_min % 60;
    snprintf(buf, cap, "UTC%c%d:%02d", west < 0 ? '-' : '+', hours, mins);
    return 0;
}

time_t os_clock_now(void) {
    time_t now = time(NULL);
#if !defined(ARDUBOT_TARGET_ESP32)
    now += (time_t)g_adjust_s;
#endif
    return now;
}

int os_clock_set(time_t unix_time) {
#if defined(ARDUBOT_TARGET_ESP32)
    struct timeval tv;
    tv.tv_sec = unix_time;
    tv.tv_usec = 0;
    return settimeofday(&tv, NULL) == 0 ? 0 : -1;
#else
    g_adjust_s = (int32_t)(unix_time - time(NULL));
    return 0;
#endif
}

int os_clock_shift(int32_t delta_s) {
#if defined(ARDUBOT_TARGET_ESP32)
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return -1;
    }
    tv.tv_sec += delta_s;
    return settimeofday(&tv, NULL) == 0 ? 0 : -1;
#else
    g_adjust_s += delta_s;
    return 0;
#endif
}

void os_clock_reset(void) {
#if !defined(ARDUBOT_TARGET_ESP32)
    g_adjust_s = 0;
#endif
}
