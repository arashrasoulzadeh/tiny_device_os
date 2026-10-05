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

    app_ui_bind_keys(&g_ui, (app_ui_key_def_t[]){
        {SIM_KEY_ENTER, on_refresh, NULL},
        {SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL},
        {0, NULL, NULL},
    });

    refresh(app);

    /* Static frame (title bar, the two lines that never change after
     * boot, help bar) drawn exactly once here - on_frame only repaints
     * the one line that actually changes (uptime seconds), not the
     * whole screen. A full-screen clear-then-redraw once a second is
     * visible as a flash/flicker on a real panel with no double buffer
     * (confirmed on hardware, RISCV_TODO.md Phase 4) - repainting only
     * the changed line avoids it entirely instead of just reducing how
     * often it happens. */
    app_ui_begin_frame(&g_ui);
    app_ui_linef(&g_ui, 0, "ArdubotOS %s", g_info.target ? g_info.target : "?");
    app_ui_linef(&g_ui, 1, "Disp %ux%u", (unsigned)g_info.display_w,
                 (unsigned)g_info.display_h);
    app_ui_end_frame(&g_ui);

    APP_INFO("Info app ready");
}

static void redraw_uptime_line(uint32_t up_s, uint32_t installed_apps) {
    int row_h = app_ui_row_h(&g_ui);
    int row_y = 2 * row_h;
    /* Clear just this row's rect, not the whole screen (see on_init's
     * comment) - full row width so a shorter new value can't leave a
     * trailing fragment of a longer old one behind. */
    app_ui_rect_color(&g_ui, g_ui.ui.content_x, row_y, g_ui.ui.content_w, row_h, 0, 0x0000);
    app_ui_linef(&g_ui, 2, "Up:%us Apps:%u", (unsigned)up_s, (unsigned)installed_apps);
    app_display_flush(&g_ui.ctx.display); /* app_ui_end_frame()'s job - needed on sim too */
}

static void on_frame(void* app) {
    uint32_t up_s;
    static uint32_t last_shown_up_s = UINT32_MAX;

    if (!app_is_dirty(app)) {
        return;
    }

    device_info_query(&g_info);
    up_s = g_info.uptime_ms / 1000u;

    if (up_s == last_shown_up_s) {
        return;
    }
    last_shown_up_s = up_s;

    redraw_uptime_line(up_s, g_info.installed_apps);
}

static void on_cleanup(void* app) {
    (void)app;
    app_ui_deinit(&g_ui);
}

APP_DEFINE(info_app, "info", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Device and OS information", .type = APP_TYPE_TOOL,
           .icon = &info_app_icon, .fps = 10, .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
