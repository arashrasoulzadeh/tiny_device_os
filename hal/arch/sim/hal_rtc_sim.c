#include "hal_rtc.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct hal_rtc {
    char path[64];
    bool initialized;
    hal_rtc_time_t current_time;
    hal_rtc_time_t alarm_time;
    bool alarm_set;
    void (*alarm_cb)(void* arg);
    void* alarm_arg;
    int32_t compensation_ppm;
};

hal_rtc_t* hal_rtc_open(const char* path) {
    hal_rtc_t* rtc = calloc(1, sizeof(hal_rtc_t));
    if (!rtc) return NULL;
    
    strncpy(rtc->path, path, sizeof(rtc->path) - 1);
    rtc->initialized = false;
    rtc->alarm_set = false;
    rtc->compensation_ppm = 0;
    
    // Set default time to 2024-01-01 00:00:00
    rtc->current_time.year = 24;
    rtc->current_time.month = 1;
    rtc->current_time.day = 1;
    rtc->current_time.hour = 0;
    rtc->current_time.minute = 0;
    rtc->current_time.second = 0;
    rtc->current_time.weekday = 1;
    
    return rtc;
}

void hal_rtc_close(hal_rtc_t* rtc) {
    if (rtc) free(rtc);
}

int hal_rtc_init(hal_rtc_t* rtc) {
    if (!rtc) return -1;
    rtc->initialized = true;
    return 0;
}

int hal_rtc_set_time(const hal_rtc_time_t* time) {
    if (!time) return -1;
    // In simulator, we'd need a global RTC instance
    return 0;
}

int hal_rtc_get_time(hal_rtc_time_t* time) {
    if (!time) return -1;
    // In simulator, return a fixed time
    time->year = 24;
    time->month = 1;
    time->day = 1;
    time->hour = 12;
    time->minute = 0;
    time->second = 0;
    time->weekday = 1;
    return 0;
}

int hal_rtc_set_alarm(const hal_rtc_time_t* time) {
    (void)time;
    return 0;
}

void hal_rtc_clear_alarm(void) {
}

void hal_rtc_set_alarm_callback(void (*cb)(void* arg), void* arg) {
    (void)cb; (void)arg;
}

int hal_rtc_set_compensation(hal_rtc_t* rtc, int32_t ppm) {
    if (!rtc) return -1;
    rtc->compensation_ppm = ppm;
    return 0;
}

int32_t hal_rtc_get_compensation(const hal_rtc_t* rtc) {
    return rtc ? rtc->compensation_ppm : 0;
}

const char* hal_rtc_get_path(const hal_rtc_t* rtc) {
    return rtc ? rtc->path : NULL;
}

int hal_rtc_suspend(hal_rtc_t* rtc) {
    if (!rtc) return -1;
    rtc->initialized = false;
    return 0;
}

int hal_rtc_resume(hal_rtc_t* rtc) {
    if (!rtc) return -1;
    rtc->initialized = true;
    return 0;
}