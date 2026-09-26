#include "status.h"
#include "display.h"

#include <stddef.h>

#if !defined(ARDUBOT_PIO)
#include "app.h"
#include "app_kit.h"
#endif

static int g_battery_percent = 100;

void app_status_set_battery_percent(int percent) {
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    g_battery_percent = percent;
}

int app_status_battery_percent(void) {
    return g_battery_percent;
}

#if !defined(ARDUBOT_PIO)
static int count_running_apps(void) {
    app_t* apps[APP_MAX];
    size_t count = 0;
    int running = 0;
    size_t i;

    if (app_list(apps, APP_MAX, &count) != 0) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (apps[i] && apps[i]->state == APP_STATE_RUNNING) {
            running++;
        }
    }
    return running;
}
#endif

static void blit_battery(int x, int y, int percent,
                         void (*set_pixel)(int px, int py, bool on, void* user), void* user) {
    int bars;
    int bx;
    int by;
    int i;

    if (!set_pixel) {
        return;
    }
    bars = (percent + 24) / 25;
    if (bars > 4) {
        bars = 4;
    }

    for (bx = 0; bx < 10; bx++) {
        set_pixel(x + bx, y, true, user);
        set_pixel(x + bx, y + 6, true, user);
    }
    for (by = 1; by < 6; by++) {
        set_pixel(x, y + by, true, user);
        set_pixel(x + 9, y + by, true, user);
    }
    set_pixel(x + 10, y + 2, true, user);
    set_pixel(x + 10, y + 3, true, user);
    set_pixel(x + 10, y + 4, true, user);

    for (i = 0; i < bars; i++) {
        int fx = x + 2 + i * 2;
        for (by = 2; by <= 4; by++) {
            set_pixel(fx, y + by, true, user);
            set_pixel(fx + 1, y + by, true, user);
        }
    }
}

static void blit_apps_icon(int x, int y,
                           void (*set_pixel)(int px, int py, bool on, void* user), void* user) {
    int i;
    if (!set_pixel) {
        return;
    }
    for (i = 0; i < 3; i++) {
        set_pixel(x + i, y, true, user);
        set_pixel(x + i, y + 2, true, user);
        set_pixel(x + 4 + i, y, true, user);
        set_pixel(x + 4 + i, y + 2, true, user);
        set_pixel(x + i, y + 4, true, user);
        set_pixel(x + i, y + 6, true, user);
        set_pixel(x + 4 + i, y + 4, true, user);
        set_pixel(x + 4 + i, y + 6, true, user);
    }
}

static const uint8_t k_digit3x5[10][3] = {
    {0x1F, 0x11, 0x1F}, {0x00, 0x1F, 0x00}, {0x1D, 0x15, 0x17}, {0x15, 0x15, 0x1F},
    {0x07, 0x04, 0x1F}, {0x17, 0x15, 0x1D}, {0x1F, 0x15, 0x1D}, {0x01, 0x01, 0x1F},
    {0x1F, 0x15, 0x1F}, {0x17, 0x15, 0x1F},
};

static void blit_digit(int x, int y, int digit,
                       void (*set_pixel)(int px, int py, bool on, void* user), void* user) {
    int col;
    int row;
    if (!set_pixel || digit < 0 || digit > 9) {
        return;
    }
    for (col = 0; col < 3; col++) {
        uint8_t bits = k_digit3x5[digit][col];
        for (row = 0; row < 5; row++) {
            if (bits & (1u << row)) {
                set_pixel(x + col, y + row, true, user);
            }
        }
    }
}

void app_status_blit(int display_w, int battery_percent, int running_apps,
                     void (*set_pixel)(int x, int y, bool on, void* user), void* user) {
    int batt_x;
    int apps_x;
    int n;

    if (!set_pixel || display_w < APP_STATUS_WIDTH) {
        return;
    }

    batt_x = display_w - 12;
    apps_x = batt_x - 14;

    n = running_apps;
    if (n < 0) {
        n = 0;
    }
    if (n > 9) {
        n = 9;
    }

    blit_apps_icon(apps_x, 0, set_pixel, user);
    blit_digit(apps_x + 8, 1, n, set_pixel, user);
    blit_battery(batt_x, 0, battery_percent, set_pixel, user);
}

#if !defined(ARDUBOT_PIO)
static void status_set_pixel(int x, int y, bool on, void* user) {
    app_ctx_t* app = (app_ctx_t*)user;
    if (!app || !on) {
        return;
    }
    app_pixel(app, x, y, true);
}

void app_status_draw(app_ctx_t* app) {
    if (!app || !app_kit_is_foreground(app)) {
        return;
    }
    app_status_blit(APP_DISPLAY_WIDTH, g_battery_percent, count_running_apps(), status_set_pixel,
                    app);
}
#else
void app_status_draw(app_ctx_t* app) {
    (void)app;
}
#endif
