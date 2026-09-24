#include "os_time.h"
#include "scheduler.h"
#include <stdlib.h>

static time_us_t g_boot_time = 0;
static time_us_t g_time_offset = 0;
static timer_t* g_timer_list = NULL;

time_us_t time_now_us(void) {
    return g_time_offset + g_boot_time;
}

time_ms_t time_now_ms(void) {
    return (time_ms_t)(time_now_us() / 1000);
}

time_us_t time_since_us(time_us_t start) {
    time_us_t now = time_now_us();
    return (now >= start) ? (now - start) : (TIME_US_MAX - start + now);
}

time_ms_t time_since_ms(time_ms_t start) {
    time_ms_t now = time_now_ms();
    return (now >= start) ? (now - start) : (TIME_MS_MAX - start + now);
}

void time_sleep_us(uint32_t us) {
    time_us_t start = time_now_us();
    while (time_since_us(start) < us) {
        task_yield();
    }
}

void time_sleep_ms(uint32_t ms) {
    time_sleep_us(ms * 1000);
}

int timer_create(timer_t* timer, time_us_t delay_us, timer_callback_t cb, void* arg, bool periodic) {
    if (!timer || !cb) {
        return -1;
    }
    
    timer->expire_time = time_now_us() + delay_us;
    timer->callback = cb;
    timer->arg = arg;
    timer->periodic = periodic;
    timer->period = delay_us;
    timer->next = NULL;
    timer->active = false;
    
    return 0;
}

static void timer_list_insert(timer_t* timer) {
    timer_t** current = &g_timer_list;
    while (*current && (*current)->expire_time <= timer->expire_time) {
        current = &(*current)->next;
    }
    timer->next = *current;
    *current = timer;
}

int timer_start(timer_t* timer) {
    if (!timer || timer->active) {
        return -1;
    }
    
    timer->expire_time = time_now_us() + (timer->periodic ? timer->period : 0);
    timer->active = true;
    timer_list_insert(timer);
    
    return 0;
}

int timer_stop(timer_t* timer) {
    if (!timer || !timer->active) {
        return -1;
    }
    
    timer_t** current = &g_timer_list;
    while (*current) {
        if (*current == timer) {
            *current = timer->next;
            timer->next = NULL;
            timer->active = false;
            return 0;
        }
        current = &(*current)->next;
    }
    
    return -1;
}

int timer_delete(timer_t* timer) {
    timer_stop(timer);
    timer->callback = NULL;
    timer->arg = NULL;
    return 0;
}

void timers_process(void) {
    time_us_t now = time_now_us();
    
    while (g_timer_list && g_timer_list->expire_time <= now) {
        timer_t* timer = g_timer_list;
        g_timer_list = timer->next;
        timer->next = NULL;
        
        if (timer->periodic) {
            timer->expire_time = now + timer->period;
            timer_list_insert(timer);
        } else {
            timer->active = false;
        }
        
        if (timer->callback) {
            timer->callback(timer->arg);
        }
    }
}

int time_rtc_init(void) {
    return 0;
}

int time_rtc_set(time_us_t epoch_us) {
    g_time_offset = epoch_us - g_boot_time;
    return 0;
}

time_us_t time_rtc_get(void) {
    return time_now_us();
}

bool time_rtc_is_valid(void) {
    return g_time_offset != 0;
}