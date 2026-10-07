#pragma once

/**
 * Status band for every standard app, including the launcher.
 *
 * Battery on the left, uptime clock in the center, separator on the last
 * row. header_app.c is the only place that paints it. app_display_flush
 * reads that paint when the screen is visible. .game and .fullscreen
 * leave the band off.
 */

#include "header.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void header_app_set_visible(bool visible);
bool header_app_visible(void);

/** Paint the band when this screen is a standard app. */
void header_app_composite(app_display_t* disp);

#ifdef __cplusplus
}
#endif
