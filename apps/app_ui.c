#include "app_ui.h"
#include "app_kit.h"
#include "app_framework.h"
#include "ssd1306_model.h"
#include "menu.h"
#include <stdlib.h>
#include <stdio.h>

extern int g_next_pin;

int app_ui_init(app_ui_t* app, void* real_app, const app_ui_config_t* cfg) {
    if (!app || !cfg) return -1;

    memset(app, 0, sizeof(*app));
    /* app_ui_text()/app_ui_textf()/app_ui_pixel()/app_ui_begin_frame() all
     * read app->ui (the top-level config), not app->ctx.ui - without this,
     * content_h stays 0 and every "draw_y < content_h" bounds check fails,
     * so nothing ever draws (e.g. the pitch-black screen after opening an
     * app whose on_frame uses the app_ui_t/g_ui pattern). */
    app->ui = *cfg;
    app->ctx.ui = *cfg;
    app->ctx.dirty = false;
    /* app_kit_run() passes the real, registered app_ctx_t here; without
     * linking desc, app_open()/app_request_exit() on this ctx see desc as
     * NULL and silently no-op (e.g. Enter doing nothing in the launcher). */
    if (real_app) {
        app->ctx.desc = ((app_ctx_t*)real_app)->desc;
    }

    if (app_display_init(&app->ctx.display, "/dev/display0") != 0) {
        return -1;
    }
    app_timer_init(&app->ctx.timer, 30);
    return 0;
}

void app_ui_deinit(app_ui_t* app) {
    if (app->gestures) {
        input_recognizer_destroy(app->gestures);
        app->gestures = NULL;
    }
    app_display_deinit(&app->ctx.display);
}

static void ui_key_trampoline(int pin, void* arg) {
    app_key_binding_t* binding = (app_key_binding_t*)arg;
    (void)pin;
    if (!binding || !binding->fn || !binding->app) {
        return;
    }
    /* Key bindings are never torn down when an app is merely suspended
     * (just backgrounded via app_open()/app_request_exit(), not fully
     * exited) - its pins stay registered. Without this check, every key
     * press reaches every app that ever bound that key, not just the
     * focused one: navigating inside a later-opened app also silently
     * drives whichever background app is still listening (e.g. the
     * launcher's own menu selection), and a later Enter press fires that
     * background app's handler too - which is how opening an unrelated
     * app out of nowhere, or two apps' frames alternating on screen, kept
     * happening. kit_key_trampoline() in app_kit.c already guards this the
     * same way; this is the same mechanism's other binding path. */
    /* binding->app is &g_ui.ctx, a different struct instance from the real
     * app_ctx_t that app_kit_run() focuses - compare by desc (shared
     * between both via app_ui_init()) instead of by pointer. */
    if (!app_kit_is_foreground_desc(((app_ctx_t*)binding->app)->desc)) {
        return;
    }
    binding->fn((app_ctx_t*)binding->app, binding->user);
}

int app_ui_bind_key(app_ui_t* app, sim_key_t key, app_key_fn_t fn, void* user) {
    if (!app || !fn) return -1;
    if (app->ctx.key_count >= APP_KIT_MAX_KEYS) return -1;
    
    app_key_binding_t* binding = &app->ctx.keys[app->ctx.key_count];
    binding->key = key;
    binding->fn = fn;
    binding->user = user;
    binding->app = (struct app_ctx*)app;  // Set app context directly
    
    int pin = g_next_pin++;
    app_button_t btn = APP_KEY_BUTTON(pin, key, ui_key_trampoline, NULL, binding);
    if (app_button_init(&btn) != 0) return -1;
    
    sim_gpio_set_key_mapping(key, pin, true);
    
    app->ctx.key_count++;
    return 0;
}

static input_key_t sim_key_to_input_key(sim_key_t key) {
    switch (key) {
        case SIM_KEY_UP:     return INPUT_KEY_UP;
        case SIM_KEY_DOWN:   return INPUT_KEY_DOWN;
        case SIM_KEY_LEFT:   return INPUT_KEY_LEFT;
        case SIM_KEY_RIGHT:  return INPUT_KEY_RIGHT;
        case SIM_KEY_ENTER:  return INPUT_KEY_ENTER;
        case SIM_KEY_ESCAPE: return INPUT_KEY_ESCAPE;
        case SIM_KEY_SPACE:  return INPUT_KEY_SPACE;
        case SIM_KEY_A: return INPUT_KEY_A; case SIM_KEY_B: return INPUT_KEY_B;
        case SIM_KEY_C: return INPUT_KEY_C; case SIM_KEY_D: return INPUT_KEY_D;
        default: return INPUT_KEY_UNKNOWN;
    }
}

typedef struct {
    app_ui_t* app;
    sim_key_t key;
} gesture_binding_t;

/* Bridges apps/input.c's global device-callback dispatch to this app_ui_t's
 * own recognizer - input_process_events() delivers to devices, not
 * directly to a recognizer, so every app_ui_t with a gesture binding
 * registers one small device whose only job is this forward. */
static void gesture_device_cb(const input_event_t* event, void* arg) {
    app_ui_t* app = (app_ui_t*)arg;
    input_recognizer_dispatch(app->gestures, event);
}

/* Single trampoline for both edges of a gesture-bound key (HAL_GPIO_IRQ_BOTH -
 * app_ui_bind_key()'s own trampoline is press-only on purpose, see the
 * comment on APP_KEY_BUTTON; gestures need both edges to tell a tap from a
 * hold, so this one reads the pin itself instead of relying on separate
 * press/release callback slots). */
static void gesture_key_trampoline(int pin, void* arg) {
    gesture_binding_t* gb = (gesture_binding_t*)arg;
    if (!gb || !gb->app) return;
    if (!app_kit_is_foreground_desc(((app_ctx_t*)gb->app)->desc)) return;

    bool pressed = sim_gpio_read(pin);
    input_event_t ev = {0};
    ev.type = pressed ? INPUT_EVENT_KEY_DOWN : INPUT_EVENT_KEY_UP;
    ev.key = sim_key_to_input_key(gb->key);
    input_post_event(&ev);
    input_process_events();
}

int app_ui_bind_gesture(app_ui_t* app, sim_key_t key, input_event_type_t gesture,
                         input_callback_t cb, void* user) {
    if (!app || !cb) return -1;

    if (!app->gestures) {
        app->gestures = input_recognizer_create();
        if (!app->gestures) return -1;

        char devname[32];
        snprintf(devname, sizeof(devname), "gestures_%p", (void*)app);
        if (input_device_register(INPUT_DEV_BUTTONS, devname, gesture_device_cb, app) != 0) {
            input_recognizer_destroy(app->gestures);
            app->gestures = NULL;
            return -1;
        }
    }

    if (input_recognizer_add_gesture(app->gestures, gesture, cb, user) != 0) return -1;

    /* One recognizer already fans out to every gesture type registered
     * for it (a single posted event only matches entries whose gesture
     * equals its type), so binding e.g. both TAP and LONG_TAP on the same
     * key only needs ONE underlying GPIO pin/trampoline - a second one
     * would make sim_gpio_handle_key() (which fires every pin mapped to a
     * given key) run the trampoline, and so post+process the same key
     * event, twice per physical press. */
    for (int i = 0; i < app->gesture_gpio_key_count; i++) {
        if (app->gesture_gpio_keys[i] == key) return 0;
    }

    gesture_binding_t* gb = calloc(1, sizeof(gesture_binding_t));
    if (!gb) return -1;
    gb->app = app;
    gb->key = key;

    int pin = g_next_pin++;
    app_button_t btn = {
        .pin = pin, .key = key, .trigger = HAL_GPIO_IRQ_BOTH,
        .on_press = gesture_key_trampoline, .on_release = NULL, .arg = gb,
    };
    if (app_button_init(&btn) != 0) {
        free(gb);
        return -1;
    }
    sim_gpio_set_key_mapping(key, pin, true);

    if (app->gesture_gpio_key_count < APP_KIT_MAX_KEYS) {
        app->gesture_gpio_keys[app->gesture_gpio_key_count++] = key;
    }

    return 0;
}

void app_ui_clear(app_ui_t* app) {
    app_display_clear(&app->ctx.display);
}

void app_ui_begin_frame(app_ui_t* app) {
    app_display_clear(&app->ctx.display);
    
    if (app->ui.mode == APP_UI_MODE_UI && app->ui.show_top_bar && app->ui.title[0]) {
        int bar_h = 8 * app->ui.text_scale;
        app_display_rect(&app->ctx.display, 0, 0, APP_DISPLAY_WIDTH, bar_h, true);
        if (app->ui.text_scale > 1) {
            ssd1306_model_draw_text_scaled(2, 0, app->ui.title, app->ui.text_scale);
        } else {
            app_display_text(&app->ctx.display, 2, 0, app->ui.title);
        }
    }
}

void app_ui_end_frame(app_ui_t* app) {
    if (app->ui.mode == APP_UI_MODE_UI && app->ui.show_help_bar && app->ui.help_text[0]) {
        int help_y = APP_DISPLAY_HEIGHT - 8 * app->ui.text_scale;
        if (app->ui.text_scale > 1) {
            ssd1306_model_draw_text_scaled(0, help_y, app->ui.help_text, app->ui.text_scale);
        } else {
            app_display_text(&app->ctx.display, 0, help_y, app->ui.help_text);
        }
    }
    app_display_flush(&app->ctx.display);
}

void app_ui_text(app_ui_t* app, int x, int y, const char* text) {
    if (!text || !app) return;
    // Validate pointer is in valid user space (not kernel space)
    if ((uintptr_t)text > 0x7FFFFFFFFFFF) return;
    int draw_y = y + app->ui.content_y;
    int draw_x = x + app->ui.content_x;
    if (draw_y >= 0 && draw_y < app->ui.content_h) {
        if (app->ui.text_scale > 1) {
            ssd1306_model_draw_text_scaled(draw_x, draw_y, text, app->ui.text_scale);
        } else {
            app_display_text(&app->ctx.display, draw_x, draw_y, text);
        }
    }
}

void app_ui_textf(app_ui_t* app, int x, int y, const char* fmt, ...) {
    if (!fmt || !app) return;
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    app_ui_text(app, x, y, buf);
}

void app_ui_line(app_ui_t* app, int row, const char* text) {
    app_ui_text(app, 0, row * app_ui_row_h(app), text);
}

void app_ui_linef(app_ui_t* app, int row, const char* fmt, ...) {
    if (!fmt || !app) return;
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    app_ui_line(app, row, buf);
}

int app_ui_bind_keys(app_ui_t* app, const app_ui_key_def_t* defs) {
    if (!app || !defs) return -1;
    int n = 0;
    for (; defs[n].fn != NULL; n++) {
        if (app_ui_bind_key(app, defs[n].key, defs[n].fn, defs[n].user) != 0) {
            return -1;
        }
    }
    return n;
}

void app_ui_pixel(app_ui_t* app, int x, int y, bool on) {
    int draw_y = y + app->ui.content_y;
    if (draw_y >= 0 && draw_y < app->ui.content_h) {
        app_display_pixel(&app->ctx.display, x, draw_y, on);
    }
}

void app_ui_rect(app_ui_t* app, int x, int y, int w, int h, bool fill) {
    int draw_y = y + app->ui.content_y;
    if (draw_y >= 0 && draw_y + h <= app->ui.content_h) {
        app_display_rect(&app->ctx.display, x, draw_y, w, h, fill);
    }
}

void app_ui_mark_dirty(app_ui_t* app) {
    if (app) {
        app->ctx.dirty = true;
    }
}

void app_ui_clear_dirty(app_ui_t* app) {
    if (app) {
        app->ctx.dirty = false;
    }
}

bool app_ui_is_dirty(const app_ui_t* app) {
    return app ? app->ctx.dirty : false;
}

static void app_ui_menu_move(app_ui_t* app, app_menu_t* menu, int delta, void* user) {
    if (!app || !menu || delta == 0) {
        return;
    }

    int visible;
    int next;

    if (menu->count <= 0 || delta == 0) {
        return;
    }

    next = menu->selected + delta;
    while (next < 0) {
        next += menu->count;
    }
    while (next >= menu->count) {
        next -= menu->count;
    }
    if (next == menu->selected) {
        return;
    }

    menu->selected = next;

    if (menu->layout != APP_MENU_LAYOUT_ICONS) {
        int avail = app->ui.content_h - menu->start_y - 8;
        int rows = avail / menu->row_h;
        if (rows < 0) rows = 0;
        if (rows > menu->count) rows = menu->count;
        visible = rows;
        if (visible > 0) {
            if (menu->selected < menu->first_visible) {
                menu->first_visible = menu->selected;
            } else if (menu->selected >= menu->first_visible + visible) {
                menu->first_visible = menu->selected - visible + 1;
            }
        }
    }

    app_ui_mark_dirty(app);
}

void app_ui_menu_nav_next(void* app, void* user) {
    app_ui_menu_move((app_ui_t*)app, (app_menu_t*)user, APP_MENU_ONE_DOWN, user);
}

void app_ui_menu_nav_prev(void* app, void* user) {
    app_ui_menu_move((app_ui_t*)app, (app_menu_t*)user, APP_MENU_ONE_UP, user);
}
