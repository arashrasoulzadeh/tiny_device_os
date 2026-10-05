#include "status.h"
#include "display.h"

#include <stddef.h>

#if defined(ARDUBOT_SIM_SDL2)
#include "device_secrets.h"
#if ARDUBOT_WIFI_HAS_CREDS
#include "hal_wifi.h"
#endif
#endif

#if !defined(ARDUBOT_PIO)
#include "app.h"
#include "app_framework.h"
#include "app_kit.h"
#endif

static int g_battery_percent = 100;
static app_status_link_t g_link = APP_STATUS_LINK_OFF;
static int g_rssi = -127;

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

void app_status_set_link(app_status_link_t link, int rssi_dbm) {
    g_link = link;
    g_rssi = rssi_dbm;
}

app_status_link_t app_status_link(void) {
    return g_link;
}

int app_status_rssi(void) {
    return g_rssi;
}

bool app_status_redraw_due(uint32_t now_ms, uint32_t last_paint_ms, bool have_painted) {
    if (!have_painted || APP_STATUS_REDRAW_MS == 0) {
        return true;
    }
    return (now_ms - last_paint_ms) >= APP_STATUS_REDRAW_MS;
}

int app_status_stable_bars(int shown, int proposed, uint32_t now_ms, int* pending,
                           uint32_t* since_ms) {
    if (!pending || !since_ms || proposed == shown) {
        if (pending) {
            *pending = shown;
        }
        return shown;
    }
    if (*pending != proposed) {
        *pending = proposed;
        *since_ms = now_ms;
        return shown;
    }
    if ((now_ms - *since_ms) >= APP_STATUS_BAR_HOLD_MS) {
        return proposed;
    }
    return shown;
}

int app_status_signal_bars(app_status_link_t link, int rssi_dbm) {
    if (link != APP_STATUS_LINK_UP) {
        return 0;
    }
    if (rssi_dbm >= -55) {
        return 4;
    }
    if (rssi_dbm >= -67) {
        return 3;
    }
    if (rssi_dbm >= -75) {
        return 2;
    }
    return 1;
}

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

/* 7×7 Wi-Fi fan. `bars` is 0–4; `up` false draws the same arc with a slash. */
static void blit_wifi(int x, int y, int bars, bool up,
                      void (*set_pixel)(int px, int py, bool on, void* user), void* user) {
    if (!set_pixel) {
        return;
    }
    if (!up) {
        set_pixel(x + 1, y, true, user);
        set_pixel(x + 2, y, true, user);
        set_pixel(x + 3, y, true, user);
        set_pixel(x + 4, y, true, user);
        set_pixel(x + 5, y, true, user);
        set_pixel(x, y + 1, true, user);
        set_pixel(x + 6, y + 1, true, user);
        set_pixel(x + 5, y + 1, true, user);
        set_pixel(x + 4, y + 2, true, user);
        set_pixel(x + 3, y + 3, true, user);
        set_pixel(x + 2, y + 4, true, user);
        set_pixel(x + 1, y + 5, true, user);
        return;
    }
    if (bars >= 4) {
        set_pixel(x + 1, y, true, user);
        set_pixel(x + 2, y, true, user);
        set_pixel(x + 3, y, true, user);
        set_pixel(x + 4, y, true, user);
        set_pixel(x + 5, y, true, user);
        set_pixel(x, y + 1, true, user);
        set_pixel(x + 6, y + 1, true, user);
    }
    if (bars >= 3) {
        set_pixel(x + 2, y + 2, true, user);
        set_pixel(x + 4, y + 2, true, user);
    }
    if (bars >= 2) {
        set_pixel(x + 3, y + 3, true, user);
    }
    if (bars >= 1) {
        set_pixel(x + 3, y + 4, true, user);
        set_pixel(x + 3, y + 5, true, user);
    }
}

/* Four ascending bars, 1px wide, 1px apart. Unfilled bars keep a bottom stub. */
static void blit_signal(int x, int y, int bars,
                        void (*set_pixel)(int px, int py, bool on, void* user), void* user) {
    static const int k_height[4] = {2, 3, 5, 7};
    int i;

    if (!set_pixel) {
        return;
    }
    if (bars < 0) {
        bars = 0;
    }
    if (bars > 4) {
        bars = 4;
    }
    for (i = 0; i < 4; i++) {
        int h = (i < bars) ? k_height[i] : 1;
        int top = 7 - h;
        int dy;
        int bx = x + i * 2;
        for (dy = top; dy <= 6; dy++) {
            set_pixel(bx, y + dy, true, user);
        }
    }
}

void app_status_blit(int display_w, int battery_percent, app_status_link_t link, int rssi_dbm,
                     void (*set_pixel)(int x, int y, bool on, void* user), void* user) {
    int bars;
    bool up;

    if (!set_pixel || display_w < APP_STATUS_WIDTH) {
        return;
    }

    up = (link == APP_STATUS_LINK_UP);
    bars = app_status_signal_bars(link, rssi_dbm);
    blit_wifi(APP_STATUS_WIFI_X(display_w), 0, bars, up, set_pixel, user);
    blit_signal(APP_STATUS_SIGNAL_X(display_w), 0, bars, set_pixel, user);
    blit_battery(APP_STATUS_BATT_X(display_w), 0, battery_percent, set_pixel, user);
}

#if !defined(ARDUBOT_PIO)
#if defined(ARDUBOT_SIM_SDL2)
static void ensure_sim_link(void) {
    static int done = 0;
    if (done) {
        return;
    }
    done = 1;
#if ARDUBOT_WIFI_HAS_CREDS
    {
        hal_wifi_t* wifi = hal_wifi_open("/dev/wifi0");
        int rssi;
        if (!wifi || hal_wifi_init(wifi) != 0 || hal_wifi_start(wifi) != 0 ||
            hal_wifi_connect(wifi, ARDUBOT_WIFI_SSID, ARDUBOT_WIFI_PASSWORD) != 0) {
            if (wifi) {
                hal_wifi_close(wifi);
            }
            app_status_set_link(APP_STATUS_LINK_DOWN, -127);
            return;
        }
        rssi = hal_wifi_get_rssi(wifi);
        hal_wifi_close(wifi);
        app_status_set_link(APP_STATUS_LINK_UP, rssi);
        APP_INFO("WiFi %s rssi=%d", ARDUBOT_WIFI_SSID, rssi);
    }
#else
    app_status_set_link(APP_STATUS_LINK_OFF, -127);
#endif
}
#endif

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
#if defined(ARDUBOT_SIM_SDL2)
    ensure_sim_link();
#endif
    app_status_blit(APP_DISPLAY_WIDTH, g_battery_percent, g_link, g_rssi, status_set_pixel, app);
}
#else
void app_status_draw(app_ctx_t* app) {
    (void)app;
}
#endif
