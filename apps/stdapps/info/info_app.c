#include "app_framework.h"
#include "app_helper.h"
#include "device_info.h"

#include <stdint.h>

extern const app_icon_t info_app_icon;

static device_info_t g_info;
static uint32_t g_shown_up_s = UINT32_MAX;

static void refresh(void) {
    device_info_query(&g_info);
}

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    if (ev == APP_EV_SELECT) {
        refresh();
        g_shown_up_s = UINT32_MAX;
    }
}

static void on_tick(app_helper_t* app) {
    uint32_t up_s;
    refresh();
    up_s = g_info.uptime_ms / 1000u;
    if (up_s == g_shown_up_s) {
        return;
    }
    g_shown_up_s = up_s;
    app_helper_invalidate(app);
}

static void on_draw(app_helper_t* app) {
    app_helper_labelf(app, 0, "ArdubotOS %s", g_info.target ? g_info.target : "?");
    app_helper_labelf(app, 1, "Disp %ux%u", (unsigned)g_info.display_w, (unsigned)g_info.display_h);
    app_helper_labelf(app, 2, "Up:%us Apps:%u", (unsigned)(g_info.uptime_ms / 1000u),
                      (unsigned)g_info.installed_apps);
}

static void on_ready(app_helper_t* app) {
    (void)app;
    refresh();
    APP_INFO("Info app ready");
}

APP_HELPER(info_app, "info", .version = "1.0.0", .author = "ArdubotOS", .title = "INFO",
           .help = "Sel:Refresh  Bk:Back", .description = "Device and OS information",
           .type = APP_TYPE_TOOL, .icon = &info_app_icon, .fps = 10, .on_event = on_event,
           .on_tick = on_tick, .on_ready = on_ready, .on_draw = on_draw)
