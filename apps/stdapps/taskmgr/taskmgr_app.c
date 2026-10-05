#include "app_framework.h"
#include "app_kit.h"
#include "scheduler.h"

#include <string.h>

extern const app_icon_t taskmgr_app_icon;

static app_ui_t g_ui;
static int g_scroll = 0;

static char task_state_char(task_state_t s) {
    switch (s) {
        case TASK_STATE_READY: return 'Y';
        case TASK_STATE_RUNNING: return 'R';
        case TASK_STATE_BLOCKED: return 'B';
        case TASK_STATE_SUSPENDED: return 'S';
        case TASK_STATE_TERMINATED: return 'T';
        default: return '?';
    }
}

static char task_prio_char(task_priority_t p) {
    switch (p) {
        case TASK_PRIO_IDLE: return 'I';
        case TASK_PRIO_LOW: return 'L';
        case TASK_PRIO_NORMAL: return 'N';
        case TASK_PRIO_HIGH: return 'H';
        case TASK_PRIO_CRITICAL: return 'C';
        default: return '?';
    }
}

static void on_scroll(void* app, void* user) {
    (void)app;
    (void)user;
    g_scroll++; /* clamped back into range in on_frame() */
    app_mark_dirty(NULL);
}

static void on_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TASKS", "Up:scroll  Bk:back");
    app_ui_init(&g_ui, app, &cfg);

    app_ui_bind_keys(&g_ui, (app_ui_key_def_t[]){
        {SIM_KEY_UP, on_scroll, NULL},
        {SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL},
        {0, NULL, NULL},
    });

    APP_INFO("Task Manager ready");
}

/* Real ArdubotOS kernel task list (scheduler_get_task_slots() — added
 * alongside this app specifically for this kind of introspection), not a
 * mockup. No per-task CPU% here: unlike FreeRTOS this cooperative
 * scheduler doesn't track a per-task runtime counter, so a CPU% column
 * would have to be fabricated — shown instead: name, state, priority. */
#define TASKMGR_MAX_VISIBLE_ROWS 8

static void on_frame(void* app) {
    (void)app;
    int max_slots = 0;
    const task_tcb_t* slots = scheduler_get_task_slots(&max_slots);
    int active_count = 0;
    int i;

    for (i = 0; i < max_slots; i++) {
        if (slots[i].name[0] != '\0') {
            active_count++;
        }
    }

    /* Auto-sizes to this display's actual row height/content area instead
     * of a fixed row count, same spirit as the dynamic-UI counter. */
    const int header_rows = 2;
    const int row_h = app_ui_row_h(&g_ui);
    int visible_rows = (g_ui.ui.content_h / row_h) - header_rows;
    if (visible_rows < 1) {
        visible_rows = 1;
    }
    if (visible_rows > TASKMGR_MAX_VISIBLE_ROWS) {
        visible_rows = TASKMGR_MAX_VISIBLE_ROWS;
    }
    if (g_scroll > active_count - visible_rows) {
        g_scroll = active_count - visible_rows;
    }
    if (g_scroll < 0) {
        g_scroll = 0;
    }

    /* Build the screen's text content first and only actually redraw
     * (full clear + redraw) if it differs from what's already on
     * screen - this ran unthrottled at this app's fps regardless of
     * whether anything had changed, which was visible as a flash on a
     * real panel with no double buffer (confirmed on hardware,
     * RISCV_TODO.md Phase 4/5 - same root cause info_app.c/
     * pomodoro_app.c already hit). Showing uptime in whole seconds
     * instead of the raw millisecond tick count also stops the header
     * line itself from appearing to change on every single poll. */
    char header[32];
    char rows[TASKMGR_MAX_VISIBLE_ROWS][24];
    int shown = 0;
    snprintf(header, sizeof(header), "Tasks:%d Up:%lus", active_count,
            (unsigned long)(scheduler_get_tick_count() / 1000));
    {
        int skipped = 0;
        for (i = 0; i < max_slots && shown < visible_rows; i++) {
            if (slots[i].name[0] == '\0') {
                continue;
            }
            if (skipped < g_scroll) {
                skipped++;
                continue;
            }
            snprintf(rows[shown], sizeof(rows[shown]), "%-9.9s %c %c", slots[i].name,
                     task_state_char(slots[i].state), task_prio_char(slots[i].priority));
            shown++;
        }
    }

    static char last_header[32];
    static char last_rows[TASKMGR_MAX_VISIBLE_ROWS][24];
    static int last_shown = -1;
    bool changed = (shown != last_shown) || strcmp(header, last_header) != 0;
    if (!changed) {
        for (i = 0; i < shown; i++) {
            if (strcmp(rows[i], last_rows[i]) != 0) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) {
        return;
    }
    strncpy(last_header, header, sizeof(last_header) - 1);
    last_header[sizeof(last_header) - 1] = '\0';
    for (i = 0; i < shown; i++) {
        strncpy(last_rows[i], rows[i], sizeof(last_rows[i]) - 1);
        last_rows[i][sizeof(last_rows[i]) - 1] = '\0';
    }
    last_shown = shown;

    app_ui_begin_frame(&g_ui);
    app_ui_line(&g_ui, 0, header);
    app_ui_line(&g_ui, 1, "NAME      ST P");
    for (i = 0; i < shown; i++) {
        app_ui_line(&g_ui, header_rows + i, rows[i]);
    }
    app_ui_end_frame(&g_ui);
}

static void on_cleanup(void* app) {
    (void)app;
    app_ui_deinit(&g_ui);
}

APP_DEFINE(taskmgr_app, "taskmgr", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Kernel task list (htop-style)", .type = APP_TYPE_TOOL,
           .icon = &taskmgr_app_icon, .fps = 10,
           .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
