#include "app_framework.h"
#include "app_kit.h"
#include "scheduler.h"
#include "theme.h"

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

    /* Draw the title/help bar chrome and the column-header row (which
     * never changes) exactly once here - on_frame() only ever repaints
     * the header stats line or the task rows, each in their own small
     * rect, not the whole screen. */
    app_ui_begin_frame(&g_ui);
    app_ui_line(&g_ui, 1, "NAME      ST P");
    app_ui_end_frame(&g_ui);

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

    /* Build the screen's text content first, and redraw ONLY the part
     * that actually changed (the header stats line changes every
     * second; the task rows rarely do) - each in its own small rect,
     * never a full app_ui_begin_frame() screen clear. The first attempt
     * at this only throttled HOW OFTEN a redraw happened, but a redraw
     * still cleared the whole content area every time the header's
     * uptime ticked over (every second), which is the same full-screen
     * flash info_app.c/pomodoro_app.c already had to be fixed the same
     * way (confirmed on hardware, RISCV_TODO.md Phase 4/5). Showing
     * uptime in whole seconds instead of the raw millisecond tick count
     * also stops the header from appearing to change on every poll. */
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
    static bool have_rendered = false;

    if (!have_rendered || strcmp(header, last_header) != 0) {
        strncpy(last_header, header, sizeof(last_header) - 1);
        last_header[sizeof(last_header) - 1] = '\0';
        app_ui_rect_color(&g_ui, g_ui.ui.content_x, 0, g_ui.ui.content_w, row_h, 0,
                          ARDUBOT_COLOR_BG);
        app_ui_line(&g_ui, 0, header);
        app_display_flush(&g_ui.ctx.display);
    }

    bool rows_changed = !have_rendered || shown != last_shown;
    if (!rows_changed) {
        for (i = 0; i < shown; i++) {
            if (strcmp(rows[i], last_rows[i]) != 0) {
                rows_changed = true;
                break;
            }
        }
    }
    if (rows_changed) {
        for (i = 0; i < shown; i++) {
            strncpy(last_rows[i], rows[i], sizeof(last_rows[i]) - 1);
            last_rows[i][sizeof(last_rows[i]) - 1] = '\0';
        }
        last_shown = shown;
        /* Clear the whole rows area (not just `shown` rows) so a
         * shorter new list doesn't leave stale rows from a longer old
         * one behind. app_ui_rect_color()'s own bounds check compares
         * (y + content_y + h) against content_h, not (content_y +
         * content_h) - i.e. the real usable height for a y>0 rect is
         * less than content_h - y by content_y, so cap the clear height
         * at what's actually available or it silently fails to draw at
         * all (hit this exact bug in pomodoro_app.c's panel). */
        {
            int rows_y = header_rows * row_h;
            int rows_area_h = visible_rows * row_h;
            int max_h = (g_ui.ui.content_h - g_ui.ui.content_y) - rows_y;
            if (rows_area_h > max_h) {
                rows_area_h = max_h;
            }
            app_ui_rect_color(&g_ui, g_ui.ui.content_x, rows_y, g_ui.ui.content_w, rows_area_h, 0,
                              ARDUBOT_COLOR_BG);
        }
        for (i = 0; i < shown; i++) {
            app_ui_line(&g_ui, header_rows + i, rows[i]);
        }
        app_display_flush(&g_ui.ctx.display);
    }

    have_rendered = true;
}

static void on_cleanup(void* app) {
    (void)app;
    app_ui_deinit(&g_ui);
}

APP_DEFINE(taskmgr_app, "taskmgr", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Kernel task list (htop-style)", .type = APP_TYPE_TOOL,
           .icon = &taskmgr_app_icon, .fps = 10,
           .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
