#include "sensors_app.h"
#include "app_framework.h"
#include "notify_service.h"

#include <stdio.h>

int sensors_format_line(char *buf, size_t cap, const char *key,
                        sensor_type_t type, int32_t raw) {
  int n;
  int abs_raw;
  if (!buf || cap == 0 || !key) {
    return -1;
  }
  if (type == SENSOR_TYPE_TEMP) {
    abs_raw = raw < 0 ? -raw : raw;
    n = snprintf(buf, cap, "%s %s%d.%dC", key, raw < 0 ? "-" : "", abs_raw / 10,
                 abs_raw % 10);
  } else if (type == SENSOR_TYPE_CPU || type == SENSOR_TYPE_RAM) {
    n = snprintf(buf, cap, "%s %ld%%", key, (long)raw);
  } else if (type == SENSOR_TYPE_POWER) {
    n = raw < 0 ? snprintf(buf, cap, "%s --", key)
                : snprintf(buf, cap, "%s %ldMHz", key, (long)raw);
  } else {
    n = snprintf(buf, cap, "%s %ld", key, (long)raw);
  }
  if (n < 0 || (size_t)n >= cap) {
    return -1;
  }
  return n;
}

static void on_ready(app_helper_t *app) {
  notify_spec_t spec = {
      .title = "hello world",
      .extent = NOTIFY_EXTENT_BAND,
      .duration_ms = 0,
  };
  (void)app;
  notify_post(&spec);
}

static void on_tick(app_helper_t *app) { app_helper_invalidate(app); }

static void on_view(app_helper_t *app) {
  int n = sensor_service_count();
  int i;
  if (n <= 0) {
    app_scene_row(app, 0, "No sensors");
    return;
  }
  for (i = 0; i < n && i < SENSOR_SERVICE_MAX && i < APP_SCENE_ROWS; i++) {
    int32_t value = 0;
    char line[32];
    const char *key = sensor_service_key(i);
    if (!key || sensor_get(key, &value) != 0 ||
        sensors_format_line(line, sizeof(line), key, sensor_service_type(i),
                            value) < 0) {
      app_scene_row(app, i, "%s --", key ? key : "?");
      continue;
    }
    app_scene_row(app, i, "%s", line);
  }
}

power_demand_t sensors_app_power_demand(void) { return POWER_DEMAND_LOW; }

APP_HELPER(sensors_app, "sensors", .every_ms = 500, .demand = POWER_DEMAND_LOW,
           .on_ready = on_ready, .on_tick = on_tick, .on_view = on_view)
