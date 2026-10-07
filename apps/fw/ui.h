#pragma once

/* Pixels, text, and frame timing. The font and the draw path are in fw/ui.c
 * so every app shares one copy. */

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"
#include "display.h"
#include "os_time.h"

#ifdef __cplusplus
extern "C" {
#endif

int app_display_init(app_display_t *disp, const char *dev_path);
void app_display_deinit(app_display_t *disp);
void app_display_clear(app_display_t *disp);
void app_display_text(app_display_t *disp, int x, int y, const char *text);
void app_display_flush(app_display_t *disp);
void app_display_pixel(app_display_t *disp, int x, int y, bool on);
void app_display_rect(app_display_t *disp, int x, int y, int w, int h,
                      bool fill);
void app_display_set_rotation(app_display_t *disp, uint8_t rot);
void app_display_pixel_color(app_display_t *disp, int x, int y,
                             uint16_t rgb565);
void app_display_fill_rect_color(app_display_t *disp, int x, int y, int w,
                                 int h, int radius, uint16_t rgb565);
void app_display_draw_rect_color(app_display_t *disp, int x, int y, int w,
                                 int h, int radius, uint16_t rgb565);
void app_display_fill_circle_color(app_display_t *disp, int cx, int cy, int r,
                                   uint16_t rgb565);
void app_display_hline_color(app_display_t *disp, int x, int y, int w,
                             uint16_t rgb565);
void app_display_text_color(app_display_t *disp, int x, int y, const char *text,
                            int scale, uint16_t rgb565);
int app_display_text_width(const char *text, int scale);

/* app_timer_t is defined in app_types.h. */
static inline void app_timer_init(app_timer_t *timer, uint32_t fps) {
  timer->target_fps = fps;
  timer->frame_time_ms = (fps > 0) ? (1000 / fps) : 0;
  timer->last_frame_ms = time_now_ms();
  timer->frame_count = 0;
  timer->delta_ms = 0;
}

static inline uint32_t app_timer_get_tick(void) { return time_now_ms(); }

static inline bool app_timer_should_frame(app_timer_t *timer) {
  uint32_t now = app_timer_get_tick();
  if (now - timer->last_frame_ms >= timer->frame_time_ms) {
    timer->delta_ms = now - timer->last_frame_ms;
    timer->last_frame_ms = now;
    timer->frame_count++;
    return true;
  }
  return false;
}

static inline void app_timer_sleep_remaining(app_timer_t *timer) {
  uint32_t now = app_timer_get_tick();
  uint32_t elapsed = now - timer->last_frame_ms;
  if (elapsed < timer->frame_time_ms) {
    time_sleep_ms(timer->frame_time_ms - elapsed);
  }
}

#ifdef __cplusplus
}
#endif
