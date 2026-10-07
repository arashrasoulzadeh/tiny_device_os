#include "app_framework.h"
#include "clock.h"
#include "clock_service.h"

#include <time.h>

static void read_now(int *hour, int *minute, int *second) {
  time_t now = os_clock_now();
  struct tm *t = localtime(&now);
  if (!t) {
    *hour = 0;
    *minute = 0;
    *second = 0;
    return;
  }
  *hour = t->tm_hour;
  *minute = t->tm_min;
  *second = t->tm_sec;
}

static void on_event(app_helper_t *app, app_helper_event_t ev) {
  (void)app;
  if (ev == APP_EV_UP) {
    clock_service_shift(3600);
  } else if (ev == APP_EV_SELECT) {
    clock_service_shift(60);
  }
}

static void on_tick(app_helper_t *app) {
  clock_service_checkpoint();
  app_helper_invalidate(app);
}

static void on_view(app_helper_t *app) {
  int hour;
  int minute;
  int second;
  char digits[9];
  read_now(&hour, &minute, &second);
  clock_fmt_hms(digits, sizeof(digits), hour, minute, second);
  app_scene_hero(app, digits, ARDUBOT_COLOR_TEXT);
  app_scene_row(app, 0, "Up:h  Sel:m");
}

power_demand_t clock_app_power_demand(void) { return POWER_DEMAND_LOW; }

APP_HELPER(clock_app, "clock", .every_ms = 100, .demand = POWER_DEMAND_LOW,
           .on_event = on_event, .on_tick = on_tick, .on_view = on_view)
