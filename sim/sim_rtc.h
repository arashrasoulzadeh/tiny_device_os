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
} sim_rtc_time_t;

int sim_rtc_init(void);
void sim_rtc_cleanup(void);

int sim_rtc_set_time(const sim_rtc_time_t* time);
int sim_rtc_get_time(sim_rtc_time_t* time);

int sim_rtc_set_alarm(const sim_rtc_time_t* time);
void sim_rtc_clear_alarm(void);

void sim_rtc_set_alarm_callback(void (*cb)(void* arg), void* arg);

#ifdef __cplusplus
}
#endif