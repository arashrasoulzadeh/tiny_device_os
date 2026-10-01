#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_ICON_SIZE 16
#define APP_ICON_PITCH 28 /* icon + gap for side-scroll strip */

/** 16×16 mono icon: one uint16_t per row, MSB = leftmost pixel. */
typedef struct {
    uint16_t rows[APP_ICON_SIZE];
} app_icon_t;

/** Built-in fallback when an app omits `.icon`. */
extern const app_icon_t app_icon_default;

/**
 * Blit @p icon at (x,y). If @p selected, draw a 1px frame.
 * @p set_pixel must clip out-of-range coordinates.
 * @p scale: 1 = normal, 2 = 2x, etc.
 */
void app_icon_blit(int x, int y, const app_icon_t* icon, bool selected,
                   void (*set_pixel)(int px, int py, bool on, void* user), void* user, int scale);

#ifdef __cplusplus
}
#endif
