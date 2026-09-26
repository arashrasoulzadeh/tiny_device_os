#include "app_framework.h"
#include "app.h"
#include "ssd1306_model.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_APPS 16
#define MENU_ITEM_HEIGHT 12
#define MENU_START_Y 16

static void launcher_app_entry(void);

static app_display_t g_display;
static app_timer_t g_timer;
static app_t* g_app_list[MAX_APPS];
static int g_app_count = 0;
static int g_selected_idx = 0;
static int g_first_visible = 0;
static bool g_need_redraw = true;
static int g_visible_count = 0;

static void on_up_press(int pin, void* arg) {
    (void)pin;
    (void)arg;
    if (g_selected_idx > 0) {
        g_selected_idx--;
        if (g_selected_idx < g_first_visible) {
            g_first_visible = g_selected_idx;
        }
        g_need_redraw = true;
    }
}

static void on_down_press(int pin, void* arg) {
    (void)pin;
    (void)arg;
    if (g_selected_idx < g_app_count - 1) {
        g_selected_idx++;
        if (g_selected_idx >= g_first_visible + g_visible_count) {
            g_first_visible = g_selected_idx - g_visible_count + 1;
        }
        g_need_redraw = true;
    }
}

static void on_select_press(int pin, void* arg) {
    (void)pin;
    (void)arg;
    if (g_selected_idx >= 0 && g_selected_idx < g_app_count) {
        app_t* app = g_app_list[g_selected_idx];
        if (app && app->state != APP_STATE_RUNNING) {
            app_start(app->name);
        }
    }
}

static void on_back_press(int pin, void* arg) {
    (void)pin;
    (void)arg;
    g_need_redraw = true;
}

static app_button_t g_buttons[] = {
    APP_BUTTON(1, SIM_KEY_UP, on_up_press, NULL, NULL),
    APP_BUTTON(2, SIM_KEY_DOWN, on_down_press, NULL, NULL),
    APP_BUTTON(3, SIM_KEY_ENTER, on_select_press, NULL, NULL),
    APP_BUTTON(4, SIM_KEY_ESCAPE, on_back_press, NULL, NULL),
};
static app_buttons_t g_button_set = {
    .buttons = g_buttons,
    .count = sizeof(g_buttons) / sizeof(g_buttons[0]),
};

static void draw_launcher(void) {
    char buf[64];

    app_display_clear(&g_display);
    app_display_text(&g_display, 0, 0, "=== ArdubotOS Launcher ===");

    int display_h = SSD1306_HEIGHT;
    g_visible_count = (display_h - MENU_START_Y) / MENU_ITEM_HEIGHT;
    if (g_visible_count > g_app_count) {
        g_visible_count = g_app_count;
    }

    for (int i = 0; i < g_visible_count; i++) {
        int idx = g_first_visible + i;
        if (idx >= g_app_count) {
            break;
        }

        app_t* app = g_app_list[idx];
        if (!app) {
            continue;
        }

        int y = MENU_START_Y + i * MENU_ITEM_HEIGHT;
        if (idx == g_selected_idx) {
            snprintf(buf, sizeof(buf), "> %s", app->name);
        } else {
            snprintf(buf, sizeof(buf), "  %s", app->name);
        }
        app_display_text(&g_display, 0, y, buf);

        const char* type_str = "";
        switch (app->type) {
            case APP_TYPE_SYSTEM:
                type_str = "[SYS]";
                break;
            case APP_TYPE_USER:
                type_str = "[USR]";
                break;
            case APP_TYPE_GAME:
                type_str = "[GME]";
                break;
            case APP_TYPE_TOOL:
                type_str = "[TOL]";
                break;
        }
        snprintf(buf, sizeof(buf), "  %s", type_str);
        app_display_text(&g_display, 64, y, buf);
    }

    int help_y = MENU_START_Y + g_visible_count * MENU_ITEM_HEIGHT;
    if (help_y < display_h - 8) {
        app_display_text(&g_display, 0, help_y, "Up/Dn: Navigate");
        app_display_text(&g_display, 0, help_y + 8, "Enter: Launch  Esc: Back");
    }

    app_display_flush(&g_display);
    g_need_redraw = false;
}

static void refresh_app_list(void) {
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
    g_need_redraw = true;
}

static void launcher_app_init(void) {
    if (app_display_init(&g_display, "/dev/display0") != 0) {
        APP_ERROR("Display init failed");
        return;
    }
    if (app_buttons_init(&g_button_set) != 0) {
        APP_ERROR("Button init failed");
    }
    app_timer_init(&g_timer, 30);

    refresh_app_list();
    draw_launcher();
    APP_INFO("Launcher App ready");
}

static void launcher_app_loop(void) {
    if (g_need_redraw) {
        draw_launcher();
    }
    if (app_timer_should_frame(&g_timer)) {
        app_timer_sleep_remaining(&g_timer);
    }
}

static void launcher_app_cleanup(void) {
    app_display_deinit(&g_display);
    APP_INFO("Launcher App cleaned up");
}

static app_lifecycle_t s_lifecycle = {
    .on_init = launcher_app_init,
    .on_loop = launcher_app_loop,
    .on_cleanup = launcher_app_cleanup,
};

static app_manifest_t* create_manifest(void) {
    static capability_t caps[] = APP_CAPS_BASIC;
    return app_manifest_create("launcher", "1.0.0", APP_TYPE_SYSTEM, 1, launcher_app_entry,
                               APP_STACK_SMALL, APP_HEAP_SMALL, caps,
                               (uint32_t)(sizeof(caps) / sizeof(caps[0])), "ArdubotOS",
                               "System launcher / home app");
}

static void launcher_app_entry(void) {
    if (s_lifecycle.on_init) {
        s_lifecycle.on_init();
    }
    while (1) {
        if (s_lifecycle.on_loop) {
            s_lifecycle.on_loop();
        }
        task_sleep(16);
    }
}

app_manifest_t* launcher_app_manifest = NULL;

__attribute__((constructor)) static void export_manifest(void) {
    launcher_app_manifest = create_manifest();
}

void launcher_app_tick(void) {
    if (s_lifecycle.on_loop) {
        s_lifecycle.on_loop();
    }
}
