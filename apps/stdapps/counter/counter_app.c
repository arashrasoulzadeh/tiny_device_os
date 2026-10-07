#include "app_framework.h"

#include <stdio.h>

#define COUNTER_GAUGE_RANGE                                                    \
  20 /* visual clamp only - the count itself is unbounded */

static int32_t g_count;

static void on_event(app_helper_t *app, app_helper_event_t ev) {
  (void)app;
  if (ev == APP_EV_UP) {
    g_count++;
  } else if (ev == APP_EV_SELECT) {
    g_count--;
  }
}

static void on_view(app_helper_t *app) {
  char buf[16];
  const uint16_t num_color = g_count > 0   ? ARDUBOT_COLOR_SUCCESS
                             : g_count < 0 ? ARDUBOT_COLOR_DANGER
                                           : ARDUBOT_COLOR_TEXT;
  const uint16_t fill_color =
      g_count >= 0 ? ARDUBOT_COLOR_SUCCESS : ARDUBOT_COLOR_DANGER;
  snprintf(buf, sizeof(buf), "%ld", (long)g_count);
  app_scene_hero(app, buf, num_color);
  app_scene_gauge(app, g_count, COUNTER_GAUGE_RANGE, fill_color);
}

APP_HELPER(counter_app, "counter", .state = &g_count,
           .state_size = sizeof(g_count), .on_event = on_event,
           .on_view = on_view)
