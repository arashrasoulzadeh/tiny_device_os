#include "app_ui.h"
#include "app_kit.h"
#include "app_framework.h"
#include "ssd1306_model.h"
#include "menu.h"

extern int g_next_pin;

int app_ui_init(app_ui_t* app, const app_ui_config_t* cfg) {
    if (!app || !cfg) return -1;
    
    memset(app, 0, sizeof(*app));
    app->ctx.ui = *cfg;
    app->ctx.dirty = false;
    
    if (app_display_init(&app->ctx.display, "/dev/display0") != 0) {
        return -1;
    }
    app_timer_init(&app->ctx.timer, 30);
    return 0;
}

void app_ui_deinit(app_ui_t* app) {
    app_display_deinit(&app->ctx.display);
}

static void ui_key_trampoline(int pin, void* arg) {
    app_key_binding_t* binding = (app_key_binding_t*)arg;
    (void)pin;
    if (!binding || !binding->fn) {
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
