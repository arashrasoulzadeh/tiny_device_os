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

/* State -> theme color, same semantic mapping used everywhere else
 * (success=running well, warning=blocked/waiting, danger=terminated) -
 * not a one-off taskmgr palette. */
static uint16_t task_state_color(task_state_t s) {
    switch (s) {
        case TASK_STATE_RUNNING: return ARDUBOT_COLOR_SUCCESS;
        case TASK_STATE_BLOCKED: return ARDUBOT_COLOR_WARNING;
        case TASK_STATE_SUSPENDED: return ARDUBOT_COLOR_TEXT_MUTED;
        case TASK_STATE_TERMINATED: return ARDUBOT_COLOR_DANGER;
        case TASK_STATE_READY:
        default: return ARDUBOT_COLOR_TEXT;
    }
}

/* ---------------------------------------------------------------------
 * Layout - a headline "big stat" count (apps/app_ui.c's app_ui_big_text,
 * same component counter_app.c's number and pomodoro_app.c's countdown
 * are built from) over a small uptime caption, a static column header,
 * then the scrollable task rows - same "big number + supporting detail"
 * design language as the rest of the app suite, not taskmgr's own
 * one-off plain-text layout.
 * ------------------------------------------------------------------- */
#define TASKMGR_MAX_VISIBLE_ROWS 8
#define TASKMGR_NUM_SCALE 3
#define TASKMGR_NUM_H (7 * TASKMGR_NUM_SCALE)
#define TASKMGR_CAPTION_Y (TASKMGR_NUM_H + 4)
#define TASKMGR_CAPTION_H 7
#define TASKMGR_COL_HEADER_Y (TASKMGR_CAPTION_Y + TASKMGR_CAPTION_H + 6)

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
     * the stat/caption block or the task rows, each in their own small
     * rect, not the whole screen. */
    app_ui_begin_frame(&g_ui);
    app_ui_text(&g_ui, 0, TASKMGR_COL_HEADER_Y, "NAME      ST P");
    app_ui_end_frame(&g_ui);

    APP_INFO("Task Manager ready");
}

/* Real ArdubotOS kernel task list (scheduler_get_task_slots() — added
 * alongside this app specifically for this kind of introspection), not a
 * mockup. No per-task CPU% here: unlike FreeRTOS this cooperative
 * scheduler doesn't track a per-task runtime counter, so a CPU% column
 * would have to be fabricated — shown instead: name, state, priority. */
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

    /* Auto-sizes to this display's actual remaining space below the
     * stat/caption/column-header block, same spirit as the dynamic-UI
     * counter, instead of a fixed row count. */
    const int rows_y = TASKMGR_COL_HEADER_Y + app_ui_row_h(&g_ui) + 2;
    const int row_h = app_ui_row_h(&g_ui);
    /* app_ui_rect_color()'s own bounds check compares (y + content_y + h)
     * against content_h, not (content_y + content_h) - the real usable
     * height for a y>0 rect is (content_h - content_y - y), not
     * (content_h - y) (hit this exact bug in pomodoro_app.c's panel). */
    int rows_area_h = (g_ui.ui.content_h - g_ui.ui.content_y) - rows_y;
    int visible_rows = rows_area_h / row_h;
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

    /* Build the screen's text/color content first, and redraw ONLY the
     * part that actually changed (the stat block changes every second;
     * the task rows rarely do) - each in its own small rect, never a
     * full app_ui_begin_frame() screen clear, which was visible as a
     * flash on a real panel with no double buffer (confirmed on
     * hardware, RISCV_TODO.md Phase 4/5 - same root cause info_app.c/
     * pomodoro_app.c already hit). */
    char count_str[8];
    char caption[24];
    char rows[TASKMGR_MAX_VISIBLE_ROWS][24];
    uint16_t row_colors[TASKMGR_MAX_VISIBLE_ROWS];
    int shown = 0;
    snprintf(count_str, sizeof(count_str), "%d", active_count);
    snprintf(caption, sizeof(caption), "TASKS   Up:%lus",
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
            row_colors[shown] = task_state_color(slots[i].state);
            shown++;
        }
    }

    static char last_count_str[8];
    static char last_caption[24];
    static char last_rows[TASKMGR_MAX_VISIBLE_ROWS][24];
    static int last_shown = -1;
    static bool have_rendered = false;

    bool stat_changed = !have_rendered || strcmp(count_str, last_count_str) != 0 ||
                        strcmp(caption, last_caption) != 0;
    if (stat_changed) {
        strncpy(last_count_str, count_str, sizeof(last_count_str) - 1);
        last_count_str[sizeof(last_count_str) - 1] = '\0';
        strncpy(last_caption, caption, sizeof(last_caption) - 1);
        last_caption[sizeof(last_caption) - 1] = '\0';

        app_ui_rect_color(&g_ui, g_ui.ui.content_x, 0, g_ui.ui.content_w,
                          TASKMGR_CAPTION_Y + TASKMGR_CAPTION_H, 0, ARDUBOT_COLOR_BG);
        app_ui_big_text(&g_ui, 0, count_str, TASKMGR_NUM_SCALE, ARDUBOT_COLOR_TITLE_TEXT);
        app_ui_text(&g_ui, 0, TASKMGR_CAPTION_Y, caption);
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
         * one behind. */
        app_ui_rect_color(&g_ui, g_ui.ui.content_x, rows_y, g_ui.ui.content_w,
                          visible_rows * row_h, 0, ARDUBOT_COLOR_BG);
        for (i = 0; i < shown; i++) {
            app_ui_text_color(&g_ui, 0, rows_y + i * row_h, rows[i], row_colors[i]);
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
