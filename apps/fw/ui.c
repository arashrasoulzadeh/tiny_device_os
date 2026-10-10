#include "fw/ui.h"

#include "hal_display.h"
#include "header_app.h"
#include "notify_service.h"
#include "ssd1306_model.h"

/* One copy of the panel primitives. Callers include fw/ui.h for the
 * declarations; the font and the HAL/sim branches live only in this file. */

#if defined(ARDUBOT_TARGET_ESP32) || defined(ARDUBOT_TARGET_ESP8266)
/* RISCV_TODO.md Phase 4: real-hardware display backend. Every
 * app_display_* function below routes to hal_display_* (the real driver)
 * instead of ssd1306_model_* (a pure simulator construct). One process-wide
 * singleton handle - this board only ever has one display, same
 * assumption ssd1306_model.c's own global state already makes for sim.
 * The handle and the font live in this file, so every caller shares
 * one copy. get_display() still opens the panel on first use:
 * a draw that runs before app_display_init() must not see a NULL
 * handle (confirmed on hardware, RISCV_TODO.md Phase 4/5). */
static hal_display_t *g_hw_display = NULL;

static inline hal_display_t *hw_get_display(void) {
  if (!g_hw_display) {
    g_hw_display = hal_display_open("lcd0", NULL);
    if (g_hw_display) {
      hal_display_init(g_hw_display);
    }
  }
  return g_hw_display;
}

/* hal_display.h has no text primitive (it's pixel/rect/line/bitmap only)
 * - ssd1306_model_draw_text_color()'s algorithm (5x7 font, per-dot
 * scaled blocks) is ported here rather than extending hal_display.h's
 * cross-platform contract for one board. Font table duplicated from
 * sim/models/ssd1306_model.c (itself already duplicated 3x in that file)
 * - a fourth copy for real hardware is consistent with existing practice,
 * not a new smell. */
static void hw_draw_text_color(int x, int y, const char *text, int scale,
                               uint32_t color) {
  static const uint8_t font_5x7[96][5] = {
      {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00},
      {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7F, 0x14, 0x7F, 0x14},
      {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
      {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00},
      {0x00, 0x1C, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1C, 0x00},
      {0x14, 0x08, 0x3E, 0x08, 0x14}, {0x08, 0x08, 0x3E, 0x08, 0x08},
      {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
      {0x00, 0x60, 0x60, 0x00, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
      {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
      {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
      {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
      {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x56, 0x36, 0x00, 0x00},
      {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
      {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06},
      {0x32, 0x49, 0x79, 0x41, 0x3E}, {0x7E, 0x11, 0x11, 0x11, 0x7E},
      {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
      {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41},
      {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x49, 0x49, 0x7A},
      {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
      {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41},
      {0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x0C, 0x02, 0x7F},
      {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
      {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E},
      {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31},
      {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
      {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F},
      {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07},
      {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},
      {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00},
      {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40},
      {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
      {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20},
      {0x38, 0x44, 0x44, 0x48, 0x7F}, {0x38, 0x54, 0x54, 0x54, 0x18},
      {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
      {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00},
      {0x20, 0x40, 0x44, 0x3D, 0x00}, {0x7F, 0x10, 0x28, 0x44, 0x00},
      {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
      {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38},
      {0x7C, 0x14, 0x14, 0x14, 0x08}, {0x08, 0x14, 0x14, 0x18, 0x7C},
      {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
      {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C},
      {0x1C, 0x20, 0x40, 0x20, 0x1C}, {0x3C, 0x40, 0x30, 0x40, 0x3C},
      {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
      {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00},
      {0x00, 0x00, 0x7F, 0x00, 0x00}, {0x00, 0x41, 0x36, 0x08, 0x00},
      {0x10, 0x08, 0x08, 0x10, 0x08}, {0x78, 0x46, 0x41, 0x46, 0x78},
  };
  if (!text || scale <= 0)
    return;
  int char_x = x;
  for (const char *p = text; *p; p++) {
    if (*p < 32 || *p > 126)
      continue;
    const uint8_t *glyph = font_5x7[*p - 32];
    for (int col = 0; col < 5; col++) {
      uint8_t column_data = glyph[col];
      int row = 0;
      while (row < 7) {
        if (!(column_data & (1 << row))) {
          row++;
          continue;
        }
        int run_start = row;
        while (row < 7 && (column_data & (1 << row)))
          row++;
        /* Collapse a contiguous "on" run into one fill_rect, same
         * optimization boards/esp32-c6-lcd/src/gfx_mono.h uses -
         * one real SPI window-set per run instead of per dot. */
        hal_display_fill_rect(hw_get_display(),
                              (int16_t)(char_x + col * scale),
                              (int16_t)(y + run_start * scale), (uint16_t)scale,
                              (uint16_t)((row - run_start) * scale), color);
      }
    }
    char_x += (5 + 1) * scale;
  }
}

int app_display_init(app_display_t *disp, const char *dev_path) {
  (void)dev_path; /* hal_display_open() ignores it too - this board has one
                   fixed display */
  if (!disp)
    return -1;
  if (!hw_get_display()) {
    return -1;
  }
  disp->width = APP_DISPLAY_WIDTH;
  disp->height = APP_DISPLAY_HEIGHT;
  disp->initialized = true;
  return 0;
}

void app_display_deinit(app_display_t *disp) { (void)disp; }

void app_display_clear(app_display_t *disp) {
  if (disp && disp->initialized) {
    hal_display_fill_rect(hw_get_display(), 0, 0, disp->width, disp->height,
                          0x0000);
  }
}

void app_display_text(app_display_t *disp, int x, int y, const char *text) {
  if (disp && disp->initialized)
    hw_draw_text_color(x, y, text, 1, 0xFFFFFFFF);
}

void app_display_flush(app_display_t *disp) {
  /* Pixels live in the HAL framebuffer until this call. Skipping it
   * leaves the previous frame on the glass. */
  if (disp && disp->initialized) {
    header_app_composite(disp);
    notify_service_composite(disp);
    hal_display_flush(hw_get_display());
  }
}

void app_display_pixel(app_display_t *disp, int x, int y, bool on) {
  if (disp && disp->initialized) {
    hal_display_draw_pixel(hw_get_display(), (int16_t)x, (int16_t)y,
                           on ? 0xFFFFFFFF : 0x00000000);
  }
}

void app_display_rect(app_display_t *disp, int x, int y, int w, int h,
                      bool fill) {
  if (!disp || !disp->initialized || w <= 0 || h <= 0) {
    return;
  }
  if (fill) {
    hal_display_fill_rect(hw_get_display(), (int16_t)x, (int16_t)y,
                          (uint16_t)w, (uint16_t)h, 0xFFFFFFFF);
  } else {
    hal_display_draw_rect(hw_get_display(), (int16_t)x, (int16_t)y,
                          (uint16_t)w, (uint16_t)h, 0xFFFFFFFF);
  }
}

void app_display_set_rotation(app_display_t *disp, uint8_t rot) {
  (void)disp;
  hal_display_set_rotation(hw_get_display(), (hal_display_rotation_t)rot);
}

void app_display_pixel_color(app_display_t *disp, int x, int y,
                             uint16_t rgb565) {
  if (disp && disp->initialized) {
    hal_display_draw_pixel(hw_get_display(), (int16_t)x, (int16_t)y, rgb565);
  }
}

/* `radius` (rounded corners) isn't supported by hal_display_fill_rect -
 * drawn as a plain rect. Visually minor; revisit if an app's design
 * depends on it looking rounded on real hardware. */
void app_display_fill_rect_color(app_display_t *disp, int x, int y, int w,
                                 int h, int radius, uint16_t rgb565) {
  (void)radius;
  if (disp && disp->initialized) {
    hal_display_fill_rect(hw_get_display(), (int16_t)x, (int16_t)y,
                          (uint16_t)w, (uint16_t)h, rgb565);
  }
}

void app_display_draw_rect_color(app_display_t *disp, int x, int y, int w,
                                 int h, int radius, uint16_t rgb565) {
  (void)radius;
  if (disp && disp->initialized) {
    hal_display_draw_rect(hw_get_display(), (int16_t)x, (int16_t)y,
                          (uint16_t)w, (uint16_t)h, rgb565);
  }
}

void app_display_fill_circle_color(app_display_t *disp, int cx, int cy, int r,
                                   uint16_t rgb565) {
  /* No circle primitive in hal_display.h either - approximate with a
   * bounding square fill, same "good enough for Phase 4/5" tradeoff as
   * the rect radius above. Revisit if an app's icon depends on it
   * actually looking round on real hardware. */
  if (disp && disp->initialized) {
    hal_display_fill_rect(hw_get_display(), (int16_t)(cx - r),
                          (int16_t)(cy - r), (uint16_t)(2 * r),
                          (uint16_t)(2 * r), rgb565);
  }
}

void app_display_hline_color(app_display_t *disp, int x, int y, int w,
                             uint16_t rgb565) {
  if (disp && disp->initialized) {
    hal_display_fill_rect(hw_get_display(), (int16_t)x, (int16_t)y,
                          (uint16_t)w, 1, rgb565);
  }
}

void app_display_text_color(app_display_t *disp, int x, int y, const char *text,
                            int scale, uint16_t rgb565) {
  if (disp && disp->initialized)
    hw_draw_text_color(x, y, text, scale, rgb565);
}

int app_display_text_width(const char *text, int scale) {
  if (!text || scale <= 0)
    return 0;
  int n = 0;
  for (const char *p = text; *p; p++)
    n++;
  return n * 6 * scale;
}

#else /* !ARDUBOT_TARGET_ESP32 && !ARDUBOT_TARGET_ESP8266: sim/host build, routes to ssd1306_model */

// Simple display wrapper using the build-time panel size
// (app_display_t and app_timer_t defined in app_types.h)
int app_display_init(app_display_t *disp, const char *dev_path) {
  (void)dev_path;
  if (!disp)
    return -1;
  ssd1306_model_register();
  disp->width = APP_DISPLAY_WIDTH;
  disp->height = APP_DISPLAY_HEIGHT;
  disp->initialized = true;
  return 0;
}

void app_display_deinit(app_display_t *disp) { (void)disp; }

void app_display_clear(app_display_t *disp) {
  if (disp && disp->initialized)
    ssd1306_model_clear();
}

void app_display_text(app_display_t *disp, int x, int y, const char *text) {
  if (disp && disp->initialized)
    ssd1306_model_draw_text(x, y, text);
}

void app_display_flush(app_display_t *disp) {
  if (disp && disp->initialized) {
    header_app_composite(disp);
    notify_service_composite(disp);
    ssd1306_model_render();
  }
}

void app_display_pixel(app_display_t *disp, int x, int y, bool on) {
  if (disp && disp->initialized) {
    ssd1306_model_set_pixel(x, y, on);
  }
}

void app_display_rect(app_display_t *disp, int x, int y, int w, int h,
                      bool fill) {
  int ix;
  int iy;
  if (!disp || !disp->initialized || w <= 0 || h <= 0) {
    return;
  }
  for (iy = y; iy < y + h; iy++) {
    for (ix = x; ix < x + w; ix++) {
      if (fill || ix == x || iy == y || ix == x + w - 1 || iy == y + h - 1) {
        ssd1306_model_set_pixel(ix, iy, true);
      }
    }
  }
}

void app_display_set_rotation(app_display_t *disp, uint8_t rot) {
  (void)disp;
  (void)rot;
}

/* Color variants (RGB565) — for the card-style color launcher (menu.c).
 * See ssd1306_model.c for why the color API lives in a file named after a
 * mono chip. */
void app_display_pixel_color(app_display_t *disp, int x, int y,
                             uint16_t rgb565) {
  if (disp && disp->initialized)
    ssd1306_model_set_pixel_color(x, y, rgb565);
}

void app_display_fill_rect_color(app_display_t *disp, int x, int y, int w,
                                 int h, int radius, uint16_t rgb565) {
  if (disp && disp->initialized)
    ssd1306_model_fill_rect_color(x, y, w, h, radius, rgb565);
}

void app_display_draw_rect_color(app_display_t *disp, int x, int y, int w,
                                 int h, int radius, uint16_t rgb565) {
  if (disp && disp->initialized)
    ssd1306_model_draw_rect_color(x, y, w, h, radius, rgb565);
}

void app_display_fill_circle_color(app_display_t *disp, int cx, int cy, int r,
                                   uint16_t rgb565) {
  if (disp && disp->initialized)
    ssd1306_model_fill_circle_color(cx, cy, r, rgb565);
}

void app_display_hline_color(app_display_t *disp, int x, int y, int w,
                             uint16_t rgb565) {
  if (disp && disp->initialized)
    ssd1306_model_draw_hline_color(x, y, w, rgb565);
}

void app_display_text_color(app_display_t *disp, int x, int y, const char *text,
                            int scale, uint16_t rgb565) {
  if (disp && disp->initialized)
    ssd1306_model_draw_text_color(x, y, text, scale, rgb565);
}

int app_display_text_width(const char *text, int scale) {
  return ssd1306_model_text_width(text, scale);
}

#endif /* ARDUBOT_TARGET_ESP32 || ARDUBOT_TARGET_ESP8266 */
