#include "screen.h"

bool app_screen_begin(app_ctx_t* app, const char* title) {
    if (!app || !app_is_dirty(app)) {
        return false;
    }
    app_clear(app);
    if (title) {
        app_text(app, 0, 0, title);
    }
    return true;
}

void app_screen_end(app_ctx_t* app) {
    if (!app) {
        return;
    }
    app_flush(app);
    app_clear_dirty(app);
}
