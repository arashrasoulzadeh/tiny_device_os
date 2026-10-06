#include "app_framework.h"

#include <string.h>

extern const app_icon_t taskmgr_app_icon;

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

/* Headline count over an uptime caption, a column header, then the
 * scrollable task rows. Gaps use theme spacing. Row height comes from
 * app_ui_row_h() so the caption (drawn at the app's real text scale)
 * does not overlap the header. */
#define TASKMGR_MAX_VISIBLE_ROWS 8
#define TASKMGR_NUM_SCALE 4
#define TASKMGR_NUM_H (7 * TASKMGR_NUM_SCALE)

typedef struct {
    int row_h;
    int caption_y;
    int col_header_y;
    int rows_y;
} taskmgr_layout_t;

static taskmgr_layout_t taskmgr_layout(const app_helper_t* app) {
    taskmgr_layout_t l;
    l.row_h = app_ui_row_h(&app->ui);
    l.caption_y = TASKMGR_NUM_H + ARDUBOT_SPACE_SM;
    l.col_header_y = l.caption_y + l.row_h + ARDUBOT_SPACE_MD;
    l.rows_y = l.col_header_y + l.row_h + ARDUBOT_SPACE_SM;
    return l;
}

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    if (ev == APP_EV_UP) {
        g_scroll++;
    }
}

static void on_ready(app_helper_t* app) {
    (void)app;
    APP_INFO("Task Manager ready");
}

static void on_draw(app_helper_t* app) {
    const taskmgr_layout_t L = taskmgr_layout(app);
    int max_slots = 0;
    const task_tcb_t* slots = scheduler_get_task_slots(&max_slots);
    int active_count = 0;
    int visible_rows;
    int rows_area_h;
    int shown = 0;
    int skipped = 0;
    int i;
    char count_str[8];
    char caption[24];

    for (i = 0; i < max_slots; i++) {
        if (slots[i].name[0] != '\0') {
            active_count++;
        }
    }

    rows_area_h = (app->ui.ui.content_h - app->ui.ui.content_y) - L.rows_y;
    visible_rows = rows_area_h / L.row_h;
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

    snprintf(count_str, sizeof(count_str), "%d", active_count);
    snprintf(caption, sizeof(caption), "TASKS   Up:%lus",
             (unsigned long)(scheduler_get_tick_count() / 1000));
    app_helper_number(app, 0, count_str, TASKMGR_NUM_SCALE, ARDUBOT_COLOR_TITLE_TEXT);
    app_ui_text(&app->ui, 0, L.caption_y, caption);
    app_ui_text(&app->ui, 0, L.col_header_y, "NAME      ST P");

    for (i = 0; i < max_slots && shown < visible_rows; i++) {
        char row[24];
        if (slots[i].name[0] == '\0') {
            continue;
        }
        if (skipped < g_scroll) {
            skipped++;
            continue;
        }
        snprintf(row, sizeof(row), "%-9.9s %c %c", slots[i].name, task_state_char(slots[i].state),
                 task_prio_char(slots[i].priority));
        app_ui_text_color(&app->ui, 0, L.rows_y + shown * L.row_h, row,
                          task_state_color(slots[i].state));
        shown++;
    }
}

APP_HELPER(taskmgr_app, "taskmgr", .title = "TASKS", .help = "Up:scroll  Bk:back",
           .type = APP_TYPE_TOOL, .icon = &taskmgr_app_icon, .fps = 10, .live = true, .state = &g_scroll,
           .state_size = sizeof(g_scroll), .on_event = on_event, .on_ready = on_ready,
           .on_draw = on_draw)
