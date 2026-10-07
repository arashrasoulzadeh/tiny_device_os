#include "app_framework.h"
#include "device_info.h"

#include <stdint.h>

static device_info_t g_info;

static void refresh(void) { device_info_query(&g_info); }

static void on_event(app_helper_t *app, app_helper_event_t ev) {
  (void)app;
  if (ev == APP_EV_SELECT) {
    refresh();
  }
}

static void on_tick(app_helper_t *app) {
  refresh();
  app_helper_invalidate(app);
}

static void on_view(app_helper_t *app) {
  app_scene_row(app, 0, "ArdubotOS %s", g_info.target ? g_info.target : "?");
  app_scene_row(app, 1, "Disp %ux%u", (unsigned)g_info.display_w,
                (unsigned)g_info.display_h);
  app_scene_row(app, 2, "Up:%us Apps:%u", (unsigned)(g_info.uptime_ms / 1000u),
                (unsigned)g_info.installed_apps);
  app_scene_row(app, 3, "By Arash Rasoulzadeh");
}

static void on_ready(app_helper_t *app) {
  (void)app;
  refresh();
  APP_INFO("Info app ready");
}

APP_HELPER(info_app, "info", .every_ms = 1000, .on_event = on_event,
           .on_tick = on_tick, .on_ready = on_ready, .on_view = on_view)
