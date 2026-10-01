#include "app_framework.h"
#include "app_kit.h"
#include "device_info.h"

extern const app_icon_t info_app_icon;

static device_info_t g_info;
static app_ui_t g_ui;

static void refresh(void* app) {
    (void)app;
    device_info_query(&g_info);
}

static void on_refresh(void* app, void* user) {
    (void)user; (void)app;
    refresh(app);
}

static void on_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "INFO", "Sel:Refresh  Bk:Back");
    app_ui_init(&g_ui, app, &cfg);
    
    app_ui_bind_key(&g_ui, SIM_KEY_ENTER, on_refresh, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit, NULL);
    
    refresh(app);
    APP_INFO("Info app ready");
}

static void on_frame(void* app) {
    uint32_t up_s;
    device_info_t info;
    
    if (!app_is_dirty(app)) {
        return;
    }
    
    device_info_query(&info);
    up_s = info.uptime_ms / 1000u;
    
    app_ui_begin_frame(app);
    app_ui_textf(app, 0, 0, "ArdubotOS %s", info.target ? info.target : "?");
    app_ui_textf(app, 0, 16, "Disp %ux%u", (unsigned)info.display_w,
                 (unsigned)info.display_h);
    app_ui_textf(app, 0, 24, "Up:%us Apps:%u", (unsigned)up_s,
                 (unsigned)info.installed_apps);
    app_ui_end_frame(app);
}

static void on_cleanup(void* app) {
    (void)app;
    app_ui_deinit(app);
}

APP_DEFINE(info_app, "info", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Device and OS information", .type = APP_TYPE_TOOL,
           .icon = &info_app_icon, .fps = 10, .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
