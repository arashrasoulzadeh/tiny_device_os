#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t time_us_t;
typedef uint32_t time_ms_t;

#define TIME_US_MAX UINT64_MAX
#define TIME_MS_MAX UINT32_MAX

time_us_t time_now_us(void);
time_ms_t time_now_ms(void);

time_us_t time_since_us(time_us_t start);
time_ms_t time_since_ms(time_ms_t start);

void time_sleep_us(uint32_t us);
void time_sleep_ms(uint32_t ms);

typedef void (*timer_callback_t)(void* arg);

typedef struct timer {
    time_us_t expire_time;
    timer_callback_t callback;
    void* arg;
    bool periodic;
    time_us_t period;
    struct timer* next;
    bool active;
} timer_t;

int timer_create(timer_t* timer, time_us_t delay_us, timer_callback_t cb, void* arg, bool periodic);
int timer_start(timer_t* timer);
int timer_stop(timer_t* timer);
int timer_delete(timer_t* timer);

void timers_process(void);

typedef struct {
    time_us_t boot_time;
    time_us_t last_sync;
    bool rtc_valid;
} time_rtc_t;

int time_rtc_init(void);
int time_rtc_set(time_us_t epoch_us);
time_us_t time_rtc_get(void);
bool time_rtc_is_valid(void);

#ifdef __cplusplus
}
#endif