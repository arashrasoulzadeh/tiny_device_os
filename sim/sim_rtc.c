#include "sim_rtc.h"
#include <stdlib.h>
#include <time.h>
#include <time.h>

static sim_rtc_time_t g_time = {0};
static sim_rtc_time_t g_alarm = {0};
static bool g_alarm_set = false;
static void (*g_alarm_cb)(void*) = NULL;
static void* g_alarm_arg = NULL;

int sim_rtc_init(void) {
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    
    g_time.year = tm->tm_year - 100;
    g_time.month = tm->tm_mon + 1;
    g_time.day = tm->tm_mday;
    g_time.hour = tm->tm_hour;
    g_time.minute = tm->tm_min;
    g_time.second = tm->tm_sec;
    g_time.weekday = tm->tm_wday;
    
    g_alarm_set = false;
    return 0;
}

void sim_rtc_cleanup(void) {
}

int sim_rtc_set_time(const sim_rtc_time_t* time) {
    if (!time) return -1;
    g_time = *time;
    return 0;
}

int sim_rtc_get_time(sim_rtc_time_t* time) {
    if (!time) return -1;
    *time = g_time;
    return 0;
}

int sim_rtc_set_alarm(const sim_rtc_time_t* time) {
    if (!time) return -1;
    g_alarm = *time;
    g_alarm_set = true;
    return 0;
}

void sim_rtc_clear_alarm(void) {
    g_alarm_set = false;
}

void sim_rtc_set_alarm_callback(void (*cb)(void* arg), void* arg) {
    g_alarm_cb = cb;
    g_alarm_arg = arg;
}

void sim_rtc_check_alarm(void) {
    if (!g_alarm_set || !g_alarm_cb) return;
    
    if (g_time.hour == g_alarm.hour &&
        g_time.minute == g_alarm.minute &&
        g_time.second == g_alarm.second) {
        g_alarm_cb(g_alarm_arg);
        g_alarm_set = false;
    }
}