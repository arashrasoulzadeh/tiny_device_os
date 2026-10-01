#include "app_framework.h"
#include "app_kit.h"
#include "config_store.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// --- Settings Items ---
typedef enum {
    SETTING_WIFI_SSID,
    SETTING_WIFI_PASS,
    SETTING_DISPLAY_BRIGHTNESS,
    SETTING_DISPLAY_TIMEOUT,
    SETTING_SOUND_ENABLED,
    SETTING_TIMEZONE,
    SETTING_COUNT
} setting_id_t;

static const char* setting_names[] = {
    "WiFi SSID",
    "WiFi Password",
    "Display Brightness",
    "Display Timeout",
    "Sound",
    "Timezone"
};

static const char* setting_keys[] = {
    "wifi.ssid",
    "wifi.pass",
    "display.brightness",
    "display.timeout",
    "sound.enabled",
    "timezone"
};

// --- State ---
static int g_selected = 0;
static bool g_editing = false;
static char g_edit_buffer[64];
static int g_edit_pos = 0;
static int g_last_key = 0;
static app_ui_t g_ui;

// --- Helpers ---
static void load_setting(int idx, char* buf, size_t len) {
    const char* val = app_config_get_str(setting_keys[idx], "");
    if (val && *val) {
        strncpy(buf, val, len - 1);
    } else {
        buf[0] = '\0';
    }
}

static void save_setting(int idx, const char* value) {
    app_config_set_str(setting_keys[idx], value);
    app_config_save();
}

// --- Button Handlers ---
static void on_up(void* app, void* user) {
    (void)user; (void)app;
    if (!g_editing) {
        g_selected = (g_selected > 0) ? g_selected - 1 : SETTING_COUNT - 1;
        app_mark_dirty(app);
    }
}

static void on_down(void* app, void* user) {
    (void)user; (void)app;
    if (!g_editing) {
        g_selected = (g_selected + 1) % SETTING_COUNT;
        app_mark_dirty(app);
    }
}

static void on_select(void* app, void* user) {
    (void)user; (void)app;
    if (!g_editing) {
        g_editing = true;
        load_setting(g_selected, g_edit_buffer, sizeof(g_edit_buffer));
        g_edit_pos = strlen(g_edit_buffer);
        app_mark_dirty(app);
    } else {
        static const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789 -_=./@_";
        if (g_last_key == 0) {
            g_last_key = chars[0];
        } else {
            const char* p = strchr(chars, g_last_key);
            if (p && *(p + 1)) {
                g_last_key = *(p + 1);
            } else {
                g_last_key = chars[0];
            }
        }
        
        if (g_edit_pos < (int)sizeof(g_edit_buffer) - 1) {
            memmove(&g_edit_buffer[g_edit_pos + 1], &g_edit_buffer[g_edit_pos], strlen(g_edit_buffer) - g_edit_pos + 1);
            g_edit_buffer[g_edit_pos] = g_last_key;
            g_edit_pos++;
            app_mark_dirty(app);
        }
    }
}

static void on_key_backspace(void* app, void* user) {
    (void)user; (void)app;
    if (g_editing && g_edit_pos > 0) {
        memmove(&g_edit_buffer[g_edit_pos - 1], &g_edit_buffer[g_edit_pos], strlen(g_edit_buffer) - g_edit_pos + 1);
        g_edit_pos--;
        app_mark_dirty(app);
    }
}

static void on_key_left(void* app, void* user) {
    (void)user; (void)app;
    if (g_editing && g_edit_pos > 0) {
        g_edit_pos--;
        app_mark_dirty(app);
    }
}

static void on_key_right(void* app, void* user) {
    (void)user; (void)app;
    if (g_editing && g_edit_pos < (int)strlen(g_edit_buffer)) {
        g_edit_pos++;
        app_mark_dirty(app);
    }
}

// --- Lifecycle ---
static void settings_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "SETTINGS", "Up/Dn:Nav Sel:Edit Bk:Back");
    app_ui_init(&g_ui, app, &cfg);
    
    app_ui_bind_key(&g_ui, SIM_KEY_UP, on_up, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_DOWN, on_down, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_LEFT, on_key_left, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_RIGHT, on_key_right, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, on_key_backspace, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ENTER, on_select, NULL);
    
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit, NULL);
    
    APP_INFO("Settings ready - Up/Down navigate, Enter: cycle char/select, Esc: backspace/back");
}

static void settings_frame(void* app) {
    (void)app;
    app_ui_begin_frame(&g_ui);
    
    int y = 0;
    const int line_h = 14;
    
    for (int i = 0; i < SETTING_COUNT; i++) {
        bool is_selected = (i == g_selected);
        bool is_editing = g_editing && (i == g_selected);
        
        if (y + line_h > g_ui.ui.content_h) break;
        
        // Selection indicator
        app_ui_textf(&g_ui, 0, y, "%s %s", is_selected ? ">" : " ", setting_names[i]);
        
        // Value or edit buffer
        char value[64];
        if (is_editing) {
            snprintf(value, sizeof(value), "%s_", g_edit_buffer);
        } else {
            load_setting(i, value, sizeof(value));
        }
        app_ui_text(&g_ui, 0, y + 8, value);
        
        // Cursor indicator when editing
        if (is_editing) {
            int cursor_x = 0;
            for (int j = 0; j < g_edit_pos && j < 20; j++) cursor_x += 6;
            app_ui_pixel(&g_ui, cursor_x, y + 15, true);
        }
        
        y += 18;
    }
    
    // Help text
    if (!g_editing) {
        app_ui_end_frame(&g_ui);
    } else {
        app_ui_end_frame(&g_ui);
    }
}

static void settings_cleanup(void* app) {
    (void)app;
    app_ui_deinit(&g_ui);
    APP_INFO("Settings closed");
}

APP_DEFINE(settings_app, "settings", .version = "1.0.0", .author = "ArdubotOS",
           .description = "System settings - WiFi, display, sound, timezone",
           .type = APP_TYPE_SYSTEM, .fps = 30,
           .on_init = settings_init, .on_frame = settings_frame, .on_cleanup = settings_cleanup)
