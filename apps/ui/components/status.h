#pragma once

#include "canvas.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Top-right cluster width. Layout from the right edge of the panel:
 *   Wi-Fi glyph  at x = display_w - 28 (7px)
 *   signal bars  at x = display_w - 20 (7px)
 *   battery      at x = display_w - 12 (11px)
 */
#define APP_STATUS_WIDTH 28
#define APP_STATUS_HEIGHT 8
#define APP_STATUS_WIFI_X(display_w) ((display_w) - 28)
#define APP_STATUS_SIGNAL_X(display_w) ((display_w) - 20)
#define APP_STATUS_BATT_X(display_w) ((display_w) - 12)

typedef enum {
    APP_STATUS_LINK_OFF = 0, /* no credentials / radio idle */
    APP_STATUS_LINK_DOWN,    /* enabled, not associated */
    APP_STATUS_LINK_UP
} app_status_link_t;

/**
 * Set battery level shown in the status bar (0–100).
 * USB-powered boards typically stay at 100.
 */
void app_status_set_battery_percent(int percent);
int app_status_battery_percent(void);

/** Remember Wi-Fi association and RSSI (dBm) for app_status_draw. */
void app_status_set_link(app_status_link_t link, int rssi_dbm);
app_status_link_t app_status_link(void);
int app_status_rssi(void);

/**
 * Map link + RSSI to 0–4 bars (phone-style).
 * OFF and DOWN are always 0. UP is 4 at -55 dBm or stronger, down to 1.
 */
int app_status_signal_bars(app_status_link_t link, int rssi_dbm);

/** Minimum time between full-panel paints (10 Hz). Faster and the SSD1306 flashes. */
#define APP_STATUS_REDRAW_MS 100
/** A new signal-bar count must hold this long before it may repaint. */
#define APP_STATUS_BAR_HOLD_MS 400

/**
 * True when a dirty frame may be pushed.
 * The first paint (`have_painted` false) is always due.
 */
bool app_status_redraw_due(uint32_t now_ms, uint32_t last_paint_ms, bool have_painted);

/**
 * Keep `shown` until `proposed` stays the same for APP_STATUS_BAR_HOLD_MS.
 * `pending` and `since_ms` are caller state across polls.
 */
int app_status_stable_bars(int shown, int proposed, uint32_t now_ms, int* pending,
                           uint32_t* since_ms);

/**
 * Draw Wi-Fi, signal bars, and battery in the top-right corner.
 * Safe to call after the frame content is drawn (overpaints that region).
 */
void app_status_draw(app_ctx_t* app);

/**
 * Same glyphs for board firmware that blits via a set_pixel callback.
 * @p link association state. @p rssi_dbm station RSSI (ignored unless link is UP).
 */
void app_status_blit(int display_w, int battery_percent, app_status_link_t link, int rssi_dbm,
                     void (*set_pixel)(int x, int y, bool on, void* user), void* user);

#ifdef __cplusplus
}
#endif
