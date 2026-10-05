#include "app_framework.h"
#include "app_kit.h"
#include "sim_gpio.h"
#include "theme.h"

extern const app_icon_t counter_app_icon;

#define COUNTER_MAX_SCALE 6
#define COUNTER_GAUGE_RANGE 20 /* visual clamp only - the count itself is unbounded */

static int32_t g_count;
static app_ui_t g_ui;

static void on_inc(void* app, void* user) {
    (void)user; (void)app;
    g_count++;
}

static void on_dec(void* app, void* user) {
    (void)user; (void)app;
    g_count--;
}

static void on_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "COUNTER", "Up:+  Sel:-  hold:back");
    app_ui_init(&g_ui, app, &cfg);

    app_ui_bind_keys(&g_ui, (app_ui_key_def_t[]){
        {SIM_KEY_1, on_inc, NULL},
        {SIM_KEY_2, on_dec, NULL},
        {SIM_KEY_UP, on_inc, NULL},
        {SIM_KEY_ENTER, on_dec, NULL},
        {SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL},
        {0, NULL, NULL},
    });

    APP_INFO("Counter ready");
}

/* Dynamic UI: the number's text scale is recomputed from how many digits
 * currently need to fit the content width, so it stays as large as
 * possible instead of a fixed size — ported from the equivalent logic in
 * boards/esp32-c6-lcd/src/main.cpp's draw_counter(). Color and a
 * center-anchored gauge bar both react to the value's sign too, not just
 * the number itself. */
static void on_frame(void* app) {
    (void)app;
    char buf[16];
    const int len = snprintf(buf, sizeof(buf), "%ld", (long)g_count);
    const int content_w = g_ui.ui.content_w;
    const int num_area_h = 7 * COUNTER_MAX_SCALE;
    const int bar_y = num_area_h + 6;
    const int bar_h = 8;
    int scale = content_w / (len * 6);

    if (scale > COUNTER_MAX_SCALE) {
        scale = COUNTER_MAX_SCALE;
    }
    if (scale < 1) {
        scale = 1;
    }

    app_ui_begin_frame(&g_ui);

    /* Reusable components (apps/app_ui.c) - same big-stat-number +
     * center-anchored-gauge pattern pomodoro_app.c's countdown/bar use,
     * not re-derived layout math per app. */
    {
        const uint16_t num_color = g_count > 0  ? ARDUBOT_COLOR_SUCCESS
                                   : g_count < 0 ? ARDUBOT_COLOR_DANGER
                                                 : ARDUBOT_COLOR_TEXT;
        app_ui_big_text(&g_ui, 0, buf, scale, num_color);
    }

    /* Gauge: fills from the center tick toward either side, clamped to
     * +/-COUNTER_GAUGE_RANGE for "how far from zero, roughly" at a glance -
     * the number itself has no such limit. */
    {
        const int half_w = content_w / 2 - 1;
        int32_t clamped = g_count;
        if (clamped > COUNTER_GAUGE_RANGE) {
            clamped = COUNTER_GAUGE_RANGE;
        }
        if (clamped < -COUNTER_GAUGE_RANGE) {
            clamped = -COUNTER_GAUGE_RANGE;
        }
        int fill = (int)((clamped < 0 ? -clamped : clamped) * half_w / COUNTER_GAUGE_RANGE);
        uint16_t fill_color = g_count >= 0 ? ARDUBOT_COLOR_SUCCESS : ARDUBOT_COLOR_DANGER;
        app_ui_bar_centered(&g_ui, bar_y, bar_h, g_count >= 0 ? fill : -fill, ARDUBOT_COLOR_TRACK,
                            fill_color, ARDUBOT_COLOR_TEXT);
    }

    app_ui_end_frame(&g_ui);
}

static void on_cleanup(void* app) {
    (void)app;
    app_ui_deinit(&g_ui);
}

APP_DEFINE(counter_app, "counter", .version = "2.0.0", .author = "ArdubotOS",
           .description = "Dynamic counter with sign-colored gauge", .icon = &counter_app_icon,
           .fps = 30, .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
