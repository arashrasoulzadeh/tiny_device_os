#include "app_framework.h"
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
typedef struct {
    int selected;
    bool editing;
    char edit_buffer[64];
    int edit_pos;
    int last_key;
} settings_state_t;

static settings_state_t g_st;

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
    if (!g_st.editing) {
        g_st.selected = (g_st.selected > 0) ? g_st.selected - 1 : SETTING_COUNT - 1;
        app_mark_dirty(app);
    }
}

static void on_down(void* app, void* user) {
    (void)user; (void)app;
    if (!g_st.editing) {
        g_st.selected = (g_st.selected + 1) % SETTING_COUNT;
        app_mark_dirty(app);
    }
}

static void on_select(void* app, void* user) {
    (void)user; (void)app;
    if (!g_st.editing) {
        g_st.editing = true;
        load_setting(g_st.selected, g_st.edit_buffer, sizeof(g_st.edit_buffer));
        g_st.edit_pos = strlen(g_st.edit_buffer);
        app_mark_dirty(app);
    } else {
        static const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789 -_=./@_";
        if (g_st.last_key == 0) {
            g_st.last_key = chars[0];
        } else {
            const char* p = strchr(chars, g_st.last_key);
            if (p && *(p + 1)) {
                g_st.last_key = *(p + 1);
            } else {
                g_st.last_key = chars[0];
            }
        }
        
        if (g_st.edit_pos < (int)sizeof(g_st.edit_buffer) - 1) {
            memmove(&g_st.edit_buffer[g_st.edit_pos + 1], &g_st.edit_buffer[g_st.edit_pos], strlen(g_st.edit_buffer) - g_st.edit_pos + 1);
            g_st.edit_buffer[g_st.edit_pos] = g_st.last_key;
            g_st.edit_pos++;
            app_mark_dirty(app);
        }
    }
}

static void on_key_backspace(void* app, void* user) {
    (void)user;
    if (g_st.editing) {
        if (g_st.edit_pos > 0) {
            memmove(&g_st.edit_buffer[g_st.edit_pos - 1], &g_st.edit_buffer[g_st.edit_pos], strlen(g_st.edit_buffer) - g_st.edit_pos + 1);
            g_st.edit_pos--;
        } else {
            /* Nothing left to delete - confirm and leave edit mode. Without
             * this, editing was a dead end: Up/Down no-op while editing and
             * Escape was also bound to app_request_exit, so there was no
             * way back to the list short of quitting the whole app. */
            save_setting(g_st.selected, g_st.edit_buffer);
            g_st.editing = false;
        }
        app_mark_dirty(app);
    } else {
        app_request_exit(app);
    }
}

static void on_key_left(void* app, void* user) {
    (void)user; (void)app;
    if (g_st.editing && g_st.edit_pos > 0) {
        g_st.edit_pos--;
        app_mark_dirty(app);
    }
}

static void on_key_right(void* app, void* user) {
    (void)user; (void)app;
    if (g_st.editing && g_st.edit_pos < (int)strlen(g_st.edit_buffer)) {
        g_st.edit_pos++;
        app_mark_dirty(app);
    }
}

/* on_key_backspace() owns all of Escape's behavior: backspace while
 * editing, exit-edit-mode-and-save once the buffer is empty, or quit
 * to the launcher when not editing. A second binding straight to
 * app_request_exit() here would fire on every Escape regardless of
 * editing state, fighting with backspace and making it impossible to
 * back out of edit mode without quitting the whole app. */
static const app_ui_key_def_t settings_keys[] = {
    {SIM_KEY_UP, on_up, NULL},
    {SIM_KEY_DOWN, on_down, NULL},
    {SIM_KEY_LEFT, on_key_left, NULL},
    {SIM_KEY_RIGHT, on_key_right, NULL},
    {SIM_KEY_ESCAPE, on_key_backspace, NULL},
    {SIM_KEY_ENTER, on_select, NULL},
    {0, NULL, NULL},
};

static void on_ready(app_helper_t* app) {
    (void)app;
    APP_INFO("Settings ready - Up/Down navigate, Enter: cycle char/select, Esc: backspace/back");
}

static void edit_text(char* out, size_t cap) {
    int n = (int)strlen(g_st.edit_buffer);
    int pos = g_st.edit_pos;
    if (pos < 0) {
        pos = 0;
    }
    if (pos > n) {
        pos = n;
    }
    snprintf(out, cap, "%.*s|%s", pos, g_st.edit_buffer, g_st.edit_buffer + pos);
}

static void on_draw(app_helper_t* app) {
    int i;
    int row = 0;
    for (i = 0; i < SETTING_COUNT; i++) {
        char value[sizeof(g_st.edit_buffer) + 2];
        bool selected = i == g_st.selected;
        if (g_st.editing && selected) {
            edit_text(value, sizeof(value));
        } else {
            load_setting(i, value, sizeof(value));
        }
        app_helper_labelf(app, row++, "%s %s", selected ? ">" : " ", setting_names[i]);
        app_helper_label(app, row++, value);
    }
}

static void on_cleanup(app_helper_t* app) {
    (void)app;
    APP_INFO("Settings closed");
}

APP_HELPER(settings_app, "settings", .type = APP_TYPE_SYSTEM, .live = true,
           .keys = settings_keys, .state = &g_st, .state_size = sizeof(g_st),
           .on_ready = on_ready, .on_draw = on_draw, .on_cleanup = on_cleanup)
