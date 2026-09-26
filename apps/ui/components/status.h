#pragma once

#include "canvas.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Width reserved on the right for status icons (battery + apps). */
#define APP_STATUS_WIDTH 28
#define APP_STATUS_HEIGHT 8

/**
 * Set battery level shown in the status bar (0–100).
 * USB-powered boards typically stay at 100.
 */
void app_status_set_battery_percent(int percent);
int app_status_battery_percent(void);

/**
 * Draw battery + running-apps icons in the top-right corner.
 * Safe to call after the frame content is drawn (overpaints that region).
 */
void app_status_draw(app_ctx_t* app);

/**
 * Same glyphs for board firmware that blits via a set_pixel callback.
 * @p running_apps number of apps in RUNNING state (0–9 shown as digit).
 * @p battery_percent 0–100.
 */
void app_status_blit(int display_w, int battery_percent, int running_apps,
                     void (*set_pixel)(int x, int y, bool on, void* user), void* user);

#ifdef __cplusplus
}
#endif
