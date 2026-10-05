#include "app_framework.h"
#include "app_kit.h"

#include <stddef.h>

extern const app_icon_t pomodoro_app_icon;

#define POMODORO_TOTAL_S 60 /* 1 minute for now, per the initial ask */
#define POMODORO_LOW_S 10   /* <= this many seconds left: red */
#define POMODORO_MID_S 20   /* <= this many seconds left: orange */

static int32_t g_remaining_s;
static bool g_running;
static bool g_done;
static bool g_flash_on;
static volatile bool g_worker_alive;
static task_tcb_t* g_sec_task;

static app_ui_t g_ui;

static void pomodoro_restart(void) {
    g_remaining_s = POMODORO_TOTAL_S;
    g_running = false;
    g_done = false;
    g_flash_on = false;
}

static void task_seconds(void* arg) {
    (void)arg;
    while (g_worker_alive) {
        task_sleep(1000);
        if (!g_worker_alive) {
            break;
        }
        bool changed = false;
        scheduler_lock();
        if (g_done) {
            /* Flash cadence while done: once per second is enough to read
             * as "flashing" without a second timer task. */
            g_flash_on = !g_flash_on;
            changed = true;
        } else if (g_running) {
            g_remaining_s--;
            if (g_remaining_s <= 0) {
                g_remaining_s = 0;
                g_running = false;
                g_done = true;
                g_flash_on = true;
            }
            changed = true;
        }
        scheduler_unlock();
        if (changed) {
            app_mark_dirty(NULL);
        }
    }
}

static void on_toggle_or_dismiss(void* app, void* user) {
    (void)app;
    (void)user;
    scheduler_lock();
    if (g_done) {
        pomodoro_restart();
    } else {
        g_running = !g_running;
    }
    scheduler_unlock();
    app_mark_dirty(NULL);
    APP_INFO("pomodoro %s", g_done ? "dismissed" : (g_running ? "running" : "paused"));
}

static void on_reset(void* app, void* user) {
    (void)app;
    (void)user;
    scheduler_lock();
    pomodoro_restart();
    scheduler_unlock();
    app_mark_dirty(NULL);
    APP_INFO("pomodoro reset");
}

static void on_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "POMODORO", "Up:start/pause Sel:reset");
    app_ui_init(&g_ui, app, &cfg);

    app_ui_bind_keys(&g_ui, (app_ui_key_def_t[]){
        {SIM_KEY_1, on_toggle_or_dismiss, NULL},
        {SIM_KEY_2, on_reset, NULL},
        {SIM_KEY_UP, on_toggle_or_dismiss, NULL},
        {SIM_KEY_ENTER, on_reset, NULL},
        {SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL},
        {0, NULL, NULL},
    });

    g_worker_alive = true;
    pomodoro_restart();

    if (task_create("pomo_sec", task_seconds, NULL, TASK_PRIO_NORMAL, 0, &g_sec_task) != 0) {
        APP_ERROR("Failed to create pomodoro worker task");
        g_worker_alive = false;
        return;
    }

    APP_INFO("Pomodoro ready");
}

static void on_frame(void* app) {
    (void)app;
    int32_t remaining;
    bool running;
    bool done;
    bool flash_on;

    scheduler_lock();
    remaining = g_remaining_s;
    running = g_running;
    done = g_done;
    flash_on = g_flash_on;
    scheduler_unlock();

    app_ui_begin_frame(&g_ui);

    if (done) {
        /* Full-screen flash between the done accent color and black,
         * alternating once/second via task_seconds() above. */
        const uint16_t flash_bg = flash_on ? APP_UI_RGB565(200, 0, 0) : APP_UI_RGB565(0, 0, 0);
        app_ui_rect_color(&g_ui, 0, 0, g_ui.ui.content_w, g_ui.ui.content_h, 0, flash_bg);
        app_ui_text_color(&g_ui, (g_ui.ui.content_w - 5 * 6 * 2) / 2, g_ui.ui.content_h / 2 - 10,
                          "DONE!", APP_UI_RGB565(255, 255, 255));
    } else {
        char line[8];
        uint16_t num_color = remaining <= POMODORO_LOW_S  ? APP_UI_RGB565(255, 60, 60)
                             : remaining <= POMODORO_MID_S ? APP_UI_RGB565(255, 160, 0)
                                                            : APP_UI_RGB565(255, 255, 255);
        snprintf(line, sizeof(line), "%ld", (long)remaining);
        app_ui_text_color(&g_ui, 0, 0, line, num_color);

        /* Depleting bar: full at the start, empty at zero. */
        {
            const int bar_w = g_ui.ui.content_w;
            const int bar_h = 6;
            const int bar_y = g_ui.ui.content_h / 2;
            const int fill_w = (int)(((int64_t)remaining * bar_w) / POMODORO_TOTAL_S);
            uint16_t bar_color = remaining <= POMODORO_LOW_S  ? APP_UI_RGB565(255, 60, 60)
                                 : remaining <= POMODORO_MID_S ? APP_UI_RGB565(255, 160, 0)
                                                                : APP_UI_RGB565(0, 220, 0);
            app_ui_rect_color(&g_ui, 0, bar_y, bar_w, bar_h, 0, APP_UI_RGB565(40, 40, 40));
            if (fill_w > 0) {
                app_ui_rect_color(&g_ui, 0, bar_y, fill_w, bar_h, 0, bar_color);
            }
        }

        app_ui_text(&g_ui, 0, 3, running ? "RUNNING" : "PAUSED");
    }

    app_ui_end_frame(&g_ui);
}

static void on_cleanup(void* app) {
    (void)app;
    g_worker_alive = false;
    if (g_sec_task) {
        /* A task_sleep()-based worker can be deleted directly, same as
         * stopwatch_app.c's g_sec_task — no task_resume() needed first
         * (that's only for task_suspend()-parked tasks). */
        task_delete(g_sec_task);
        g_sec_task = NULL;
    }
    app_ui_deinit(&g_ui);
    APP_INFO("Pomodoro worker stopped");
}

APP_DEFINE(pomodoro_app, "pomodoro", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Pomodoro countdown timer", .type = APP_TYPE_TOOL,
           .icon = &pomodoro_app_icon, .fps = 30,
           .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
