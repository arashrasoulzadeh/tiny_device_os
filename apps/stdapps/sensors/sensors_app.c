#include "app_framework.h"
#include "sensors_app.h"

#include <stdio.h>

extern const app_icon_t sensors_app_icon;

static int32_t g_last[SENSOR_SERVICE_MAX];
static int g_last_n = -1;

int sensors_format_line(char* buf, size_t cap, const char* key, sensor_type_t type, int32_t raw) {
    int n;
    int abs_raw;
    if (!buf || cap == 0 || !key) {
        return -1;
    }
    if (type == SENSOR_TYPE_TEMP) {
        abs_raw = raw < 0 ? -raw : raw;
        n = snprintf(buf, cap, "%s %s%d.%dC", key, raw < 0 ? "-" : "", abs_raw / 10, abs_raw % 10);
    } else {
        n = snprintf(buf, cap, "%s %ld", key, (long)raw);
    }
    if (n < 0 || (size_t)n >= cap) {
        return -1;
    }
    return n;
}

static void on_tick(app_helper_t* app) {
    int n = sensor_service_count();
    int changed = n != g_last_n;
    int i;
    for (i = 0; i < n && i < SENSOR_SERVICE_MAX; i++) {
        int32_t value = 0;
        const char* key = sensor_service_key(i);
        if (!key || sensor_get(key, &value) != 0) {
            changed = 1;
            continue;
        }
        if (g_last_n < 0 || value != g_last[i]) {
            changed = 1;
        }
        g_last[i] = value;
    }
    g_last_n = n;
    if (changed) {
        app_helper_invalidate(app);
    }
}

static void on_draw(app_helper_t* app) {
    int n = sensor_service_count();
    int i;
    if (n <= 0) {
        app_helper_label(app, 0, "No sensors");
        return;
    }
    for (i = 0; i < n && i < 6; i++) {
        int32_t value = 0;
        char line[32];
        const char* key = sensor_service_key(i);
        if (!key || sensor_get(key, &value) != 0 ||
            sensors_format_line(line, sizeof(line), key, sensor_service_type(i), value) < 0) {
            app_helper_labelf(app, i, "%s --", key ? key : "?");
            continue;
        }
        app_helper_label(app, i, line);
    }
}

APP_HELPER(sensors_app, "sensors", .title = "SENSORS", .help = "Bk:Back",
           .type = APP_TYPE_TOOL, .icon = &sensors_app_icon, .fps = 2, .on_tick = on_tick,
           .on_draw = on_draw)
