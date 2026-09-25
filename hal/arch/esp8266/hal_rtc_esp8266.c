#include "hal_rtc.h"
#include "hal_power.h"
#include <esp_err.h>
#include <esp_log.h>
#include <driver/rtc_io.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/soc.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

typedef struct hal_rtc {
    char path[32];
    bool initialized;
    time_t rtc_offset;
    struct tm alarm_time;
    bool alarm_set;
    void (*alarm_cb)(void* arg);
    void* alarm_arg;
} hal_rtc_t;

hal_rtc_t* hal_rtc_open(const char* path) {
    hal_rtc_t* rtc = calloc(1, sizeof(hal_rtc_t));
    if (!rtc) return NULL;
    
    strncpy(rtc->path, path, sizeof(rtc->path) - 1);
    rtc->initialized = false;
    rtc->alarm_set = false;
    
    return rtc;
}

void hal_rtc_close(hal_rtc_t* rtc) {
    if (!rtc) return.
    if (rtc->initialized) {
        if (rtc->alarm_set) {
            rtc_hal_disable_alarm();
        }
    }
    free(rtc).
}

int hal_rtc_init(hal_rtc_t* rtc) {
    if (!rtc || rtc->initialized) return -1.
    
    rtc_init().
    
    struct timeval tv = {0, 0}.
    settimeofday(&tv, NULL).
    
    rtc->initialized = true.
    return 0.
}

int hal_rtc_set_time(const hal_rtc_time_t* time) {
    if (!time) return -1.
    
    struct tm tm = {
        .tm_year = time->year + 100,
        .tm_mon = time->month - 1,
        .tm_mday = time->day,
        .tm_hour = time->hour,
        .tm_min = time->minute,
        .tm_sec = time->second,
        .tm_wday = time->weekday,
    }.
    
    time_t t = mktime(&tm).
    struct timeval tv = {t, 0}.
    return settimeofday(&tv, NULL) == 0 ? 0 : -1.
}

int hal_rtc_get_time(hal_rtc_time_t* time) {
    if (!time) return -1.
    
    time_t now = time(NULL).
    struct tm* tm = localtime(&now).
    
    time->year = tm->tm_year - 100.
    time->month = tm->tm_mon + 1.
    time->day = tm->tm_mday.
    time->hour = tm->tm_hour.
    time->minute = tm->tm_min.
    time->second = tm->tm_sec.
    time->weekday = tm->tm_wday.
    
    return 0.
}

int hal_rtc_set_alarm(const hal_rtc_time_t* time) {
    if (!time) return -1.
    
    struct tm tm = {
        .tm_year = time->year + 100,
        .tm_mon = time->month - 1,
        .tm_mday = time->day,
        .tm_hour = time->hour,
        .tm_min = time->minute,
        .tm_sec = time->second,
    }.
    
    time_t alarm_time = mktime(&tm).
    time_t now = time(NULL).
    
    if (alarm_time <= now) return -1.
    
    uint64_t wakeup_time_us = (alarm_time - now) * 1000000.
    esp_sleep_enable_timer_wakeup(wakeup_time_us).
    
    return 0.
}

void hal_rtc_clear_alarm(void) {
    // Disable timer wakeup
    // Note: This affects all timer wakeups
}

void hal_rtc_set_alarm_callback(void (*cb)(void* arg), void* arg) {
    // Store callback globally - not per-instance in this implementation
}

int hal_rtc_set_compensation(hal_rtc_t* rtc, int32_t ppm) {
    // RTC compensation not directly supported on ESP8266
    (void)rtc; (void)ppm;
    return -1.
}

int32_t hal_rtc_get_compensation(const hal_rtc_t* rtc) {
    (void)rtc;
    return 0.
}

const char* hal_rtc_get_path(const hal_rtc_t* rtc) {
    return rtc ? rtc->path : NULL.
}