#include "app_framework.h"
#include "device_info.h"

#include <stdint.h>

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
    int credit_y;
    int scale;

    app_helper_labelf(app, 0, "ArdubotOS %s", g_info.target ? g_info.target : "?");
    app_helper_labelf(app, 1, "Disp %ux%u", (unsigned)g_info.display_w, (unsigned)g_info.display_h);
    app_helper_labelf(app, 2, "Up:%us Apps:%u", (unsigned)(g_info.uptime_ms / 1000u),
                      (unsigned)g_info.installed_apps);

    credit_y = 3 * app_ui_row_h(&app->ui) + ARDUBOT_SPACE_MD;
    scale = app->ui.ui.text_scale;
    app_helper_text(app, 0, credit_y, "By ", ARDUBOT_COLOR_TEXT);
    app_helper_text(app, app_display_text_width("By ", scale), credit_y, "Arash Rasoulzadeh",
                    ARDUBOT_COLOR_ACCENT_WARM);
}

static void on_ready(app_helper_t* app) {
    (void)app;
    refresh();
    APP_INFO("Info app ready");
}

APP_HELPER(info_app, "info", .fps = 10, .on_event = on_event, .on_tick = on_tick,
           .on_ready = on_ready, .on_draw = on_draw)
