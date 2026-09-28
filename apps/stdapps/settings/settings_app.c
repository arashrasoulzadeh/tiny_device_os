#include "app_kit.h"
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
static app_timer_t g_timer;

// --- Helpers ---
static void load_setting(int idx, char* buf, size_t len) {
    if (app_config_get_str(setting_keys[idx], buf, len) != 0) {
        buf[0] = '\0';
    }
}

static void save_setting(int idx, const char* value) {
    app_config_set_str(setting_keys[idx], value);
    app_config_save();
}

// --- Button Handlers ---
static void on_up(app_ctx_t* app, void* user) {
    (void)user;
    if (!g_editing) {
        g_selected = (g_selected > 0) ? g_selected - 1 : SETTING_COUNT - 1;
        app_mark_dirty(app);
    }
}

static void on_down(app_ctx_t* app, void* user) {
    (void)user;
    if (!g_editing) {
        g_selected = (g_selected + 1) % SETTING_COUNT;
        app_mark_dirty(app);
    }
}

static void on_select(app_ctx_t* app, void* user) {
    (void)user;
    if (!g_editing) {
        g_editing = true;
        load_setting(g_selected, g_edit_buffer, sizeof(g_edit_buffer));
        g_edit_pos = strlen(g_edit_buffer);
        app_mark_dirty(app);
    }
}

// Character input state for text entry
static char g_last_key = 0;

static void on_key_char(app_ctx_t* app, void* user) {
    (void)user;
    if (!g_editing) return;
    
    // Get the last pressed key from app context
    // We'll use a simple approach: cycle through characters
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

static void on_key_backspace(app_ctx_t* app, void* user) {
    (void)user;
    if (g_editing && g_edit_pos > 0) {
        memmove(&g_edit_buffer[g_edit_pos - 1], &g_edit_buffer[g_edit_pos], strlen(g_edit_buffer) - g_edit_pos + 1);
        g_edit_pos--;
        app_mark_dirty(app);
    }
}

static void on_key_left(app_ctx_t* app, void* user) {
    (void)user;
    if (g_editing && g_edit_pos > 0) {
        g_edit_pos--;
        app_mark_dirty(app);
    }
}

static void on_key_right(app_ctx_t* app, void* user) {
    (void)user;
    if (g_editing && g_edit_pos < (int)strlen(g_edit_buffer)) {
        g_edit_pos++;
        app_mark_dirty(app);
    }
}

// --- Lifecycle ---
static void settings_init(app_ctx_t* app) {
    if (app_display_init(&app->display, "/dev/display0") != 0) {
        APP_ERROR("Display init failed");
        return;
    }
    
    app_timer_init(&g_timer, 30);
    
    // Bind navigation
    app_bind_key(app, SIM_KEY_UP, on_up, NULL);
    app_bind_key(app, SIM_KEY_DOWN, on_down, NULL);
    app_bind_key(app, SIM_KEY_ENTER, on_select, NULL);
    app_bind_key(app, SIM_KEY_LEFT, on_key_left, NULL);
    app_bind_key(app, SIM_KEY_RIGHT, on_key_right, NULL);
    app_bind_key(app, SIM_KEY_ESCAPE, on_key_backspace, NULL);
    app_bind_key(app, SIM_KEY_ENTER, on_key_char, NULL);
    
    app_bind_back(app);
    APP_INFO("Settings ready - Up/Down navigate, Enter: cycle char/select, Esc: backspace/back");
}

static void settings_frame(app_ctx_t* app) {
    if (!app_screen_begin(app, "SETTINGS")) return;

    char buf[64];
    int y = 8;

    for (int i = 0; i < SETTING_COUNT; i++) {
        bool is_selected = (i == g_selected);
        bool is_editing = g_editing && (i == g_selected);

        // Selection indicator
        app_textf(app, 0, y, "%s %s", is_selected ? ">" : " ", setting_names[i]);
        
        // Value or edit buffer
        char value[64];
        if (is_editing) {
            snprintf(value, sizeof(value), "%s_", g_edit_buffer);
        } else {
            load_setting(i, value, sizeof(value));
        }
        app_text(app, 0, y + 8, value);

        // Cursor indicator when editing
        if (is_editing) {
            int cursor_x = 0;
            for (int j = 0; j < g_edit_pos && j < 20; j++) cursor_x += 6;
            app_pixel(app, cursor_x, y + 15, true);
        }

        y += 18;
        if (y > SSD1306_HEIGHT - 16) break;
    }

    // Help text
    if (!g_editing) {
        app_text(app, 0, SSD1306_HEIGHT - 8, "Up/Dn:Nav Sel:Edit Bk:Back");
    } else {
        app_text(app, 0, SSD1306_HEIGHT - 8, "Type:Edit Bk:Save Esc:Cancel");
    }

    app_screen_end(app);
}

static void settings_cleanup(app_ctx_t* app) {
    app_display_deinit(&app->display);
    APP_INFO("Settings closed");
}

// --- App Definition ---
APP_DEFINE(settings_app, "settings", .version = "1.0.0", .author = "ArdubotOS",
           .description = "System settings - WiFi, display, sound, timezone",
           .type = APP_TYPE_SYSTEM, .fps = 30,
           .on_init = settings_init, .on_frame = settings_frame, .on_cleanup = settings_cleanup)