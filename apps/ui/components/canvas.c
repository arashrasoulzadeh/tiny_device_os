#include "canvas.h"
#include "app_kit.h"

#include <stdio.h>

void app_mark_dirty(app_ctx_t* app) {
    if (app) {
        app->dirty = true;
    }
}

void app_clear_dirty(app_ctx_t* app) {
    if (app) {
        app->dirty = false;
    }
}

bool app_is_dirty(const app_ctx_t* app) {
    return app ? app->dirty : false;
}

void app_clear(app_ctx_t* app) {
    if (!app || !app_kit_is_foreground(app)) {
        return;
    }
    app_display_clear(&app->display);
}

void app_text(app_ctx_t* app, int x, int y, const char* text) {
    if (!app || !text || !app_kit_is_foreground(app)) {
        return;
    }
    app_display_text(&app->display, x, y, text);
}

void app_textf(app_ctx_t* app, int x, int y, const char* fmt, ...) {
    char buf[96];
    va_list args;
    if (!app || !fmt || !app_kit_is_foreground(app)) {
        return;
    }
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    app_display_text(&app->display, x, y, buf);
}

void app_flush(app_ctx_t* app) {
    if (!app || !app_kit_is_foreground(app)) {
        return;
    }
    app_display_flush(&app->display);
    app_clear_dirty(app);
}
