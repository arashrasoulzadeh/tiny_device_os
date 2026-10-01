#include "app_framework.h"
#include "app_kit.h"
#include "sim_gpio.h"

extern const app_icon_t counter_app_icon;

static int32_t g_count;
static app_ui_t g_ui;

static void on_inc(void* app, void* user) {
    (void)user; (void)app;
    g_count++;
}

static void on_dec(void* app, void* user) {
    (void)user; (void)app;
    g_count--;
}

static void on_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "COUNTER", "Up:+  Sel:-  hold:back");
    app_ui_init(&g_ui, &cfg);
    
    app_ui_bind_key(&g_ui, SIM_KEY_1, on_inc, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_2, on_dec, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_UP, on_inc, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ENTER, on_dec, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit, NULL);
    
    APP_INFO("Counter ready");
}

static void on_frame(void* app) {
    (void)app;
    app_ui_begin_frame(&g_ui);
    app_ui_textf(&g_ui, 0, 0, "Count: %d", g_count);
    app_ui_text(&g_ui, 0, 16, "Up:+  Sel:-");
    app_ui_text(&g_ui, 0, 24, "hold Sel: back");
    app_ui_end_frame(&g_ui);
}

static void on_cleanup(void* app) {
    (void)app;
    app_ui_deinit(&g_ui);
}

APP_DEFINE(counter_app, "counter", .version = "2.0.0", .author = "ArdubotOS",
           .description = "Simple counter demo", .icon = &counter_app_icon, .fps = 30,
           .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
