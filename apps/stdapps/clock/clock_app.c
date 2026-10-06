#include "app_framework.h"
#include "app_helper.h"
#include "clock.h"
#include "icons.h"
#include "clock_service.h"

#include <stdlib.h>
#include <time.h>

extern const app_icon_t clock_app_icon;

static int g_shown_s = -1;

static void read_now(int* hour, int* minute, int* second) {
    time_t now = os_clock_now();
    struct tm* t = localtime(&now);
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

static void plot(app_helper_t* app, int x, int y, uint16_t color) {
    app_ui_pixel_color(&app->ui, app->ui.ui.content_x + x, y, color);
}

static void stroke(app_helper_t* app, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        plot(app, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    if (ev == APP_EV_UP) {
        clock_service_shift(3600);
    } else if (ev == APP_EV_SELECT) {
        clock_service_shift(60);
    }
    g_shown_s = -1;
}

static void on_tick(app_helper_t* app) {
    int hour;
    int minute;
    int second;
    read_now(&hour, &minute, &second);
    if (second == g_shown_s) {
        return;
    }
    g_shown_s = second;
    clock_service_checkpoint();
    app_helper_invalidate(app);
}

static void on_draw(app_helper_t* app) {
    int hour;
    int minute;
    int second;
    int width = app_helper_content_w(app);
    int usable = app->ui.ui.content_h - app->ui.ui.content_y;
    int side = usable < width / 2 ? usable : width / 2;
    int radius = side / 2 - 3;
    int cx = side / 2;
    int cy = usable / 2;
    int i;
    int hx;
    int hy;
    char digits[9];
    int scale;

    if (radius < 6) {
        radius = 6;
    }
    read_now(&hour, &minute, &second);
    for (i = 0; i < 60; i++) {
        int x;
        int y;
        clock_hand_end(cx, cy, radius, i, &x, &y);
        plot(app, x, y, i % 5 == 0 ? ARDUBOT_COLOR_ACCENT_COOL : ARDUBOT_COLOR_TEXT_MUTED);
    }
    clock_hand_end(cx, cy, radius * 5 / 10, clock_hour_sixtieths(hour, minute), &hx, &hy);
    stroke(app, cx, cy, hx, hy, ARDUBOT_COLOR_ACCENT_WARM);
    clock_hand_end(cx, cy, radius * 8 / 10, clock_minute_sixtieths(minute), &hx, &hy);
    stroke(app, cx, cy, hx, hy, ARDUBOT_COLOR_TEXT);
    clock_hand_end(cx, cy, radius * 9 / 10, clock_second_sixtieths(second), &hx, &hy);
    stroke(app, cx, cy, hx, hy, ARDUBOT_COLOR_DANGER);
    plot(app, cx, cy, ARDUBOT_COLOR_TITLE_TEXT);

    clock_fmt_hms(digits, sizeof(digits), hour, minute, second);
    scale = app_fit_text_scale(width - side, 8, 4);
    app_helper_center_text(app, side, 0, width - side, usable * 2 / 3, digits, scale,
                           ARDUBOT_COLOR_TEXT);
    app_helper_center_text(app, side, usable * 2 / 3, width - side, usable / 3, "Up:h  Sel:m", 1,
                           ARDUBOT_COLOR_TEXT_MUTED);
}

APP_HELPER(clock_app, "clock", .version = "1.0.0", .author = "ArdubotOS", .title = "CLOCK",
           .help = "Up:+hour  Sel:+min", .description = "Analog and digital clock",
           .type = APP_TYPE_TOOL, .icon = &clock_app_icon, .fps = 10, .on_event = on_event,
           .on_tick = on_tick, .on_draw = on_draw)
