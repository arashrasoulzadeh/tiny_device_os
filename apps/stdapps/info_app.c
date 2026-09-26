#include "app_kit.h"
#include "device_info.h"

static device_info_t g_info;

static void refresh(app_ctx_t* app) {
    device_info_query(&g_info);
    app_mark_dirty(app);
}

static void on_refresh(app_ctx_t* app, void* user) {
    (void)user;
    refresh(app);
}

static void on_init(app_ctx_t* app) {
    app_bind_key(app, SIM_KEY_ESCAPE, on_refresh, NULL);
    app_bind_key(app, SIM_KEY_ENTER, on_refresh, NULL);
    refresh(app);
    APP_INFO("Info app ready");
}

static void on_frame(app_ctx_t* app) {
    if (!app_is_dirty(app)) {
        return;
    }

    uint32_t up_s = g_info.uptime_ms / 1000u;

    app_clear(app);
    app_text(app, 0, 0, "Device Info");
    app_textf(app, 0, 10, "%s %s", g_info.os_name ? g_info.os_name : "?",
              g_info.os_version ? g_info.os_version : "?");
    app_textf(app, 0, 20, "Target: %s", g_info.target ? g_info.target : "?");
    app_textf(app, 0, 30, "Arch: %s", g_info.arch ? g_info.arch : "?");
    app_textf(app, 0, 40, "Disp: %ux%u", (unsigned)g_info.display_w,
              (unsigned)g_info.display_h);
    app_textf(app, 0, 50, "Up:%us Apps:%u", (unsigned)up_s,
              (unsigned)g_info.installed_apps);
    app_flush(app);
}

APP_DEFINE(info_app, "info", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Device and OS information", .type = APP_TYPE_TOOL, .fps = 10,
           .on_init = on_init, .on_frame = on_frame);
