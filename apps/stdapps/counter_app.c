#include "app_kit.h"

#include <stdint.h>

static int32_t g_count;

static void on_inc(app_ctx_t* app, void* user) {
    (void)user;
    g_count++;
    app_mark_dirty(app);
    APP_INFO("count=%d", g_count);
}

static void on_dec(app_ctx_t* app, void* user) {
    (void)user;
    g_count--;
    app_mark_dirty(app);
    APP_INFO("count=%d", g_count);
}

static void on_back(app_ctx_t* app, void* user) {
    (void)user;
    app_request_exit(app);
}

static void on_init(app_ctx_t* app) {
    app_bind_key(app, SIM_KEY_1, on_inc, NULL);
    app_bind_key(app, SIM_KEY_2, on_dec, NULL);
    app_bind_key(app, SIM_KEY_ESCAPE, on_back, NULL);
    APP_INFO("Counter ready — press 1 / 2, Esc to leave");
}

static void on_frame(app_ctx_t* app) {
    if (!app_is_dirty(app)) {
        return;
    }
    app_clear(app);
    app_text(app, 0, 0, "Counter");
    app_textf(app, 0, 16, "Count: %d", g_count);
    app_text(app, 0, 32, "1:+  2:-");
    app_flush(app);
}

APP_DEFINE(counter_app, "counter", .version = "2.0.0", .author = "ArdubotOS",
           .description = "Simple counter demo", .fps = 30, .on_init = on_init,
           .on_frame = on_frame);
