#include "app_kit.h"

#include <stdint.h>

extern const app_icon_t counter_app_icon;

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

static void on_init(app_ctx_t* app) {
    app_bind_key(app, SIM_KEY_1, on_inc, NULL);
    app_bind_key(app, SIM_KEY_2, on_dec, NULL);
    app_bind_key(app, SIM_KEY_UP, on_inc, NULL);
    app_bind_key(app, SIM_KEY_ENTER, on_dec, NULL);
    app_bind_back(app);
    APP_INFO("Counter ready — Up:+  Sel:-  hold:back");
}

static void on_frame(app_ctx_t* app) {
    if (!app_screen_begin(app, "Counter")) {
        return;
    }
    app_textf(app, 0, 8, "Count: %d", g_count);
    app_text(app, 0, 16, "Up:+  Sel:-");
    app_text(app, 0, 24, "hold Sel: back");
    app_screen_end(app);
}

APP_DEFINE(counter_app, "counter", .version = "2.0.0", .author = "ArdubotOS",
           .description = "Simple counter demo", .icon = &counter_app_icon, .fps = 30,
           .on_init = on_init, .on_frame = on_frame)
