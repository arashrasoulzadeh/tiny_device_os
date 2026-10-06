#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RGB565 frame the panel is not allowed to see until flush. Drawing
 * straight to the ST7789 shows the clear, then the new pixels, which
 * reads as flicker. Callers composite here and present the dirty rows
 * in one transfer. */
typedef struct display_fb {
    uint16_t* px;
    int16_t w;
    int16_t h;
    int16_t dirty_y0; /* inclusive */
    int16_t dirty_y1; /* exclusive */
    bool dirty;
} display_fb_t;

void display_fb_init(display_fb_t* fb, uint16_t* px, int16_t w, int16_t h);

void display_fb_fill_rect(display_fb_t* fb, int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t rgb565);
void display_fb_draw_pixel(display_fb_t* fb, int16_t x, int16_t y, uint16_t rgb565);
void display_fb_draw_line(display_fb_t* fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          uint16_t rgb565);

/* Full-width row span covering everything drawn since the last clear.
 * The span is contiguous in px, so one bitmap blit can send it. */
bool display_fb_dirty_rows(const display_fb_t* fb, int16_t* y, int16_t* rows);
void display_fb_clear_dirty(display_fb_t* fb);

#ifdef __cplusplus
}
#endif
