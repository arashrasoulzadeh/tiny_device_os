#include "app_framework.h"
#include "app_kit.h"
#include "stopwatch.h"

#include <stddef.h>

extern const app_icon_t stopwatch_app_icon;

static stopwatch_t g_sw;
static volatile bool g_workers_alive;

static task_tcb_t* g_sec_task;
static task_tcb_t* g_min_task;
static task_tcb_t* g_hour_task;

void stopwatch_reset(stopwatch_t* sw) {
    if (!sw) {
        return;
    }
    sw->hours = 0;
    sw->minutes = 0;
    sw->seconds = 0;
    sw->running = false;
}

bool stopwatch_tick_second(stopwatch_t* sw) {
    if (!sw || !sw->running) {
        return false;
    }
    sw->seconds++;
    if (sw->seconds >= 60) {
        sw->seconds = 0;
        return true;
    }
    return false;
}

bool stopwatch_tick_minute(stopwatch_t* sw) {
    if (!sw || !sw->running) {
        return false;
    }
    sw->minutes++;
    if (sw->minutes >= 60) {
        sw->minutes = 0;
        return true;
    }
    return false;
}

void stopwatch_tick_hour(stopwatch_t* sw) {
    if (!sw || !sw->running) {
        return;
    }
    sw->hours++;
    if (sw->hours >= 100) {
        sw->hours = 0;
    }
}

static void notify_dirty(void* app) {
    if (app) {
        app_mark_dirty(app);
    }
}

static void task_seconds(void* arg) {
    (void)arg;
    while (g_workers_alive) {
        task_sleep(1000);
        if (!g_workers_alive) {
            break;
        }
        bool minute_due = false;
        bool changed = false;
        scheduler_lock();
        if (g_sw.running) {
            minute_due = stopwatch_tick_second(&g_sw);
            changed = true;
        }
        scheduler_unlock();
        if (minute_due && g_min_task) {
            task_resume(g_min_task);
        }
        if (changed) {
            notify_dirty(NULL);
        }
    }
}

static void task_minutes(void* arg) {
    (void)arg;
    while (g_workers_alive) {
        task_suspend(task_get_current());
        if (!g_workers_alive) {
            break;
        }
        bool hour_due = false;
        scheduler_lock();
        if (g_sw.running) {
            hour_due = stopwatch_tick_minute(&g_sw);
        }
        scheduler_unlock();
        if (hour_due && g_hour_task) {
            task_resume(g_hour_task);
        }
        notify_dirty(NULL);
    }
}

static void task_hours(void* arg) {
    (void)arg;
    while (g_workers_alive) {
        task_suspend(task_get_current());
        if (!g_workers_alive) {
            break;
        }
        scheduler_lock();
        stopwatch_tick_hour(&g_sw);
        scheduler_unlock();
        notify_dirty(NULL);
    }
}

static void delete_workers(void) {
    g_workers_alive = false;
    if (g_min_task) {
        task_resume(g_min_task);
    }
    if (g_hour_task) {
        task_resume(g_hour_task);
    }
    if (g_sec_task) {
        task_delete(g_sec_task);
        g_sec_task = NULL;
    }
    if (g_min_task) {
        task_delete(g_min_task);
        g_min_task = NULL;
    }
    if (g_hour_task) {
        task_delete(g_hour_task);
        g_hour_task = NULL;
    }
}

static app_ui_t g_ui;

static void on_toggle(void* app, void* user) {
    (void)user; (void)app;
    scheduler_lock();
    g_sw.running = !g_sw.running;
    scheduler_unlock();
    app_mark_dirty(NULL);
    APP_INFO("stopwatch %s", g_sw.running ? "running" : "stopped");
}

static void on_reset(void* app, void* user) {
    (void)user; (void)app;
    scheduler_lock();
    stopwatch_reset(&g_sw);
    scheduler_unlock();
    app_mark_dirty(NULL);
    APP_INFO("stopwatch reset");
}

static void on_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "STOPWATCH", "Up:start/stop Sel:reset Bk:back");
    app_ui_init(&g_ui, app, &cfg);
    
    app_ui_bind_keys(&g_ui, (app_ui_key_def_t[]){
        {SIM_KEY_1, on_toggle, NULL},
        {SIM_KEY_2, on_reset, NULL},
        {SIM_KEY_UP, on_toggle, NULL},
        {SIM_KEY_ENTER, on_reset, NULL},
        {SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit, NULL},
        {0, NULL, NULL},
    });

    g_workers_alive = true;
    stopwatch_reset(&g_sw);
    
    if (task_create("sw_sec", task_seconds, NULL, TASK_PRIO_NORMAL, 0, &g_sec_task) != 0 ||
        task_create("sw_min", task_minutes, NULL, TASK_PRIO_NORMAL, 0, &g_min_task) != 0 ||
        task_create("sw_hour", task_hours, NULL, TASK_PRIO_NORMAL, 0, &g_hour_task) != 0) {
        APP_ERROR("Failed to create stopwatch worker tasks");
        delete_workers();
        return;
    }
    
    APP_INFO("Stopwatch ready");
}

static void on_frame(void* app) {
    (void)app;
    app_ui_begin_frame(&g_ui);
    
    uint8_t h = g_sw.hours;
    uint8_t m = g_sw.minutes;
    uint8_t s = g_sw.seconds;
    bool running = g_sw.running;
    
    app_ui_linef(&g_ui, 0, "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    app_ui_line(&g_ui, 2, running ? "RUN" : "STP");
    app_ui_line(&g_ui, 3, "Up:tog Sel:rst");
    app_ui_end_frame(&g_ui);
}

static void on_cleanup(void* app) {
    (void)app;
    delete_workers();
    app_ui_deinit(&g_ui);
    APP_INFO("Stopwatch workers stopped");
}

APP_DEFINE(stopwatch_app, "stopwatch", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Multithread stopwatch (sec/min/hour tasks)", .type = APP_TYPE_TOOL,
           .icon = &stopwatch_app_icon, .fps = 30,
           .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
