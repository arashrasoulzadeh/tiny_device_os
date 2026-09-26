#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
} hal_rtc_time_t;

typedef struct hal_rtc hal_rtc_t;

hal_rtc_t* hal_rtc_open(const char* path);
void hal_rtc_close(hal_rtc_t* rtc);

int hal_rtc_init(hal_rtc_t* rtc);
int hal_rtc_set_time(const hal_rtc_time_t* time);
int hal_rtc_get_time(hal_rtc_time_t* time);

int hal_rtc_set_alarm(const hal_rtc_time_t* time);
void hal_rtc_clear_alarm(void);

void hal_rtc_set_alarm_callback(void (*cb)(void* arg), void* arg);

int hal_rtc_set_compensation(hal_rtc_t* rtc, int32_t ppm);
int32_t hal_rtc_get_compensation(const hal_rtc_t* rtc);

const char* hal_rtc_get_path(const hal_rtc_t* rtc);

// Power management
int hal_rtc_suspend(hal_rtc_t* rtc);
int hal_rtc_resume(hal_rtc_t* rtc);

#ifdef __cplusplus
}
#endif