#include "app_helper.h"

#include <stdio.h>

extern const app_icon_t counter_app_icon;

#define COUNTER_MAX_SCALE 6
#define COUNTER_GAUGE_RANGE 20 /* visual clamp only - the count itself is unbounded */

static int32_t g_count;

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    if (ev == APP_EV_UP) {
        g_count++;
    } else if (ev == APP_EV_SELECT) {
        g_count--;
    }
}

static void on_draw(app_helper_t* app) {
    char buf[16];
    const int len = snprintf(buf, sizeof(buf), "%ld", (long)g_count);
    const int content_w = app_helper_content_w(app);
    const int scale = app_fit_text_scale(content_w, len, COUNTER_MAX_SCALE);
    const int bar_y = 7 * COUNTER_MAX_SCALE + 6;
    const uint16_t num_color = g_count > 0    ? ARDUBOT_COLOR_SUCCESS
                               : g_count < 0  ? ARDUBOT_COLOR_DANGER
                                              : ARDUBOT_COLOR_TEXT;
    const uint16_t fill_color = g_count >= 0 ? ARDUBOT_COLOR_SUCCESS : ARDUBOT_COLOR_DANGER;
    const int half_w = content_w / 2 - 1;

    app_helper_number(app, 0, buf, scale, num_color);
    app_helper_gauge(app, bar_y, 8, app_gauge_fill_px(g_count, COUNTER_GAUGE_RANGE, half_w),
                   fill_color);
}

APP_HELPER(counter_app, "counter", .version = "2.0.0", .title = "COUNTER",
         .help = "Up:+  Sel:-  hold:back",
         .description = "Dynamic counter with sign-colored gauge", .icon = &counter_app_icon,
         .on_event = on_event, .on_draw = on_draw)
