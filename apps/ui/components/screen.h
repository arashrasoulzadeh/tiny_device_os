#pragma once

#include "canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dirty-gated frame start: if the app is dirty, clear the canvas and draw
 * @p title at (0,0), then return true. Otherwise return false (skip the frame).
 */
bool app_screen_begin(app_ctx_t* app, const char* title);

/** Flush and clear dirty. Pair with a successful app_screen_begin. */
void app_screen_end(app_ctx_t* app);

#ifdef __cplusplus
}
#endif
