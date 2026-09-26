#include "app_kit.h"
#include "app.h"

#include <stdio.h>
#include <string.h>

#define MAX_APPS 16
#define MENU_ITEM_HEIGHT 12
#define MENU_START_Y 16

static app_t* g_app_list[MAX_APPS];
static int g_app_count = 0;
static int g_selected_idx = 0;
static int g_first_visible = 0;
static int g_visible_count = 0;

static const char* type_label(app_type_t type) {
    switch (type) {
        case APP_TYPE_SYSTEM:
            return "[SYS]";
        case APP_TYPE_USER:
            return "[USR]";
        case APP_TYPE_GAME:
            return "[GME]";
        case APP_TYPE_TOOL:
            return "[TOL]";
        default:
            return "";
    }
}

static void refresh_app_list(app_ctx_t* app) {
    g_app_count = 0;
    app_t* apps[MAX_APPS];
    size_t count = 0;

    if (app_list(apps, MAX_APPS, &count) == 0) {
        for (size_t i = 0; i < count && i < MAX_APPS; i++) {
            if (apps[i] && strcmp(apps[i]->name, "launcher") != 0) {
                g_app_list[g_app_count++] = apps[i];
            }
        }
    }

    if (g_selected_idx >= g_app_count) {
        g_selected_idx = g_app_count > 0 ? g_app_count - 1 : 0;
    }
    if (g_first_visible >= g_app_count) {
        g_first_visible = 0;
    }
    app_mark_dirty(app);
}

static void on_up(app_ctx_t* app, void* user) {
    (void)user;
    if (g_selected_idx > 0) {
        g_selected_idx--;
        if (g_selected_idx < g_first_visible) {
            g_first_visible = g_selected_idx;
        }
        app_mark_dirty(app);
    }
}

static void on_down(app_ctx_t* app, void* user) {
    (void)user;
    if (g_selected_idx < g_app_count - 1) {
        g_selected_idx++;
        if (g_selected_idx >= g_first_visible + g_visible_count) {
            g_first_visible = g_selected_idx - g_visible_count + 1;
        }
        app_mark_dirty(app);
    }
}

static void on_select(app_ctx_t* app, void* user) {
    (void)app;
    (void)user;
    if (g_selected_idx >= 0 && g_selected_idx < g_app_count) {
        app_t* selected = g_app_list[g_selected_idx];
        if (selected && selected->state != APP_STATE_RUNNING) {
            APP_INFO("Launching %s", selected->name);
            app_start(selected->name);
        }
    }
}

static void on_back(app_ctx_t* app, void* user) {
    (void)user;
    refresh_app_list(app);
}

static void on_init(app_ctx_t* app) {
    app_bind_key(app, SIM_KEY_UP, on_up, NULL);
    app_bind_key(app, SIM_KEY_DOWN, on_down, NULL);
    app_bind_key(app, SIM_KEY_ENTER, on_select, NULL);
    app_bind_key(app, SIM_KEY_ESCAPE, on_back, NULL);
    refresh_app_list(app);
    APP_INFO("Launcher ready");
}

static void on_frame(app_ctx_t* app) {
    if (!app_is_dirty(app)) {
        return;
    }

    int display_h = SSD1306_HEIGHT;
    g_visible_count = (display_h - MENU_START_Y) / MENU_ITEM_HEIGHT;
    if (g_visible_count > g_app_count) {
        g_visible_count = g_app_count;
    }

    app_clear(app);
    app_text(app, 0, 0, "ArdubotOS Launcher");

    for (int i = 0; i < g_visible_count; i++) {
        int idx = g_first_visible + i;
        if (idx >= g_app_count || !g_app_list[idx]) {
            break;
        }
        int y = MENU_START_Y + i * MENU_ITEM_HEIGHT;
        app_textf(app, 0, y, "%c %s", (idx == g_selected_idx) ? '>' : ' ', g_app_list[idx]->name);
        app_text(app, 72, y, type_label(g_app_list[idx]->type));
    }

    int help_y = MENU_START_Y + g_visible_count * MENU_ITEM_HEIGHT;
    if (help_y < display_h - 8) {
        app_text(app, 0, help_y, "Up/Dn Enter Esc");
    }

    app_flush(app);
}

APP_DEFINE(launcher_app, "launcher", .version = "1.0.0", .author = "ArdubotOS",
           .description = "System launcher / home app", .type = APP_TYPE_SYSTEM, .fps = 30,
           .on_init = on_init, .on_frame = on_frame);
