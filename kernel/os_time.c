#include "os_time.h"
#include "scheduler.h"
#include <stdlib.h>

/* On host/sim builds nothing ever drives time forward via time_set_now_us(),
 * so time_now_us() reads the real host clock instead of a frozen offset.
 * Board targets still rely on their platform tick feeding time_set_now_us(). */
#if defined(ARDUBOT_SIM_SDL2)
#if defined(_WIN32)
#include <windows.h>
static time_us_t host_monotonic_us(void) {
    static LARGE_INTEGER freq;
    static bool have_freq = false;
    LARGE_INTEGER counter;
    if (!have_freq) {
        QueryPerformanceFrequency(&freq);
        have_freq = true;
    }
    QueryPerformanceCounter(&counter);
    return (time_us_t)((counter.QuadPart * 1000000ULL) / (uint64_t)freq.QuadPart);
}
#else
#include <time.h>
static time_us_t host_monotonic_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (time_us_t)ts.tv_sec * 1000000ULL + (time_us_t)(ts.tv_nsec / 1000);
}
#endif
#endif

static time_us_t g_boot_time = 0;
static time_us_t g_time_offset = 0;
static timer_t* g_timer_list = NULL;

time_us_t time_now_us(void) {
#if defined(ARDUBOT_SIM_SDL2)
    static time_us_t host_epoch_us = 0;
    static bool host_epoch_init = false;
    if (!host_epoch_init) {
        host_epoch_us = host_monotonic_us();
        host_epoch_init = true;
    }
    return g_time_offset + (host_monotonic_us() - host_epoch_us);
#else
    return g_time_offset + g_boot_time;
#endif
}

time_ms_t time_now_ms(void) {
    return (time_ms_t)(time_now_us() / 1000);
}

void time_set_now_us(time_us_t now_us) {
    g_boot_time = now_us;
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