#pragma once

#include "app_types.h"
#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Status band painted by header_app.c: battery on the left, uptime clock
 * in the center, separator on the last row. Panels smaller than
 * APP_HEADER_MIN_WIDTH by APP_HEADER_MIN_HEIGHT get no band.
 * APP_HEADER_BAND is the reserved height when the band is shown.
 * Every standard app, including the launcher, shows it. .game and
 * .fullscreen do not.
 */
#define APP_HEADER_MIN_WIDTH 280
#define APP_HEADER_MIN_HEIGHT 150
#define APP_HEADER_BAND 25

/** 0 when this panel is too small for the band, otherwise APP_HEADER_BAND. */
int app_header_height(void);

/**
 * Paint the band at y = 0. Fills the whole band first, so a later call
 * covers anything the app drew there. No-op when app_header_height() is 0.
 * Screens do not call this themselves: app_display_flush reads it from
 * header_app.c while the screen is a standard app.
 */
void app_header_draw(app_display_t* disp);

#ifdef __cplusplus
}
#endif
