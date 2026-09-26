#include "app_kit.h"
#include "device_info.h"

extern const app_icon_t info_app_icon;

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
    app_bind_key(app, SIM_KEY_ENTER, on_refresh, NULL);
    app_bind_back(app);
    refresh(app);
    APP_INFO("Info app ready");
}

static void on_frame(app_ctx_t* app) {
    uint32_t up_s;

    /* Keep uptime live like the board firmware. */
    refresh(app);

    if (!app_screen_begin(app, "Device Info")) {
        return;
    }

    up_s = g_info.uptime_ms / 1000u;
    app_textf(app, 0, 8, "ArdubotOS %s", g_info.target ? g_info.target : "?");
    app_textf(app, 0, 16, "Disp %ux%u", (unsigned)g_info.display_w,
              (unsigned)g_info.display_h);
    app_textf(app, 0, 24, "Up:%us Apps:%u", (unsigned)up_s,
              (unsigned)g_info.installed_apps);
    app_screen_end(app);
}

APP_DEFINE(info_app, "info", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Device and OS information", .type = APP_TYPE_TOOL,
           .icon = &info_app_icon, .fps = 10, .on_init = on_init, .on_frame = on_frame)
