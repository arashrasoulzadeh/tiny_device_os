#pragma once

/* UI sub-framework: pixels, text, and frame timing. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "hal_display.h"
#include "os_time.h"
#include "ssd1306_model.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DISPLAY HELPERS
// ============================================================================

#if defined(ARDUBOT_TARGET_ESP32)
/* RISCV_TODO.md Phase 4: real-hardware display backend. Every
 * app_display_* function below routes to hal_display_* (the real ST7789
 * driver, hal/arch/esp32/hal_display_esp32_arduino.cpp) instead of
 * ssd1306_model_* (a pure simulator construct). One process-wide
 * singleton handle - this board only ever has one display, same
 * assumption ssd1306_model.c's own global state already makes for sim.
 *
 * This is still a per-translation-unit static (one copy per .c file
 * that includes this header), so every ESP32 app_display_* function
 * below calls esp32_get_display() rather than touching this variable
 * directly - it lazily opens/inits the real (process-wide, via
 * hal_display_esp32_arduino.cpp's own file-scope singleton) display on
 * first use in THIS translation unit, rather than assuming some other
 * file already called app_display_init() first. A stdapp that calls
 * app_display_text_color()/etc directly (bypassing the app_ui_*
 * wrappers, which go through app_ui.c's own already-initialized copy)
 * would otherwise silently no-op forever on a NULL display (confirmed
 * on hardware, RISCV_TODO.md Phase 4/5: pomodoro_app.c's countdown
 * number never appeared). */
static hal_display_t* g_esp32_display = NULL;

static inline hal_display_t* esp32_get_display(void) {
    if (!g_esp32_display) {
        g_esp32_display = hal_display_open("lcd0", NULL);
        if (g_esp32_display) {
            hal_display_init(g_esp32_display); /* idempotent - see hal_display_esp32_arduino.cpp */
        }
    }
    return g_esp32_display;
}

/* hal_display.h has no text primitive (it's pixel/rect/line/bitmap only)
 * - ssd1306_model_draw_text_color()'s algorithm (5x7 font, per-dot
 * scaled blocks) is ported here rather than extending hal_display.h's
 * cross-platform contract for one board. Font table duplicated from
 * sim/models/ssd1306_model.c (itself already duplicated 3x in that file)
 * - a fourth copy for real hardware is consistent with existing practice,
 * not a new smell. */
static void esp32_draw_text_color(int x, int y, const char* text, int scale, uint16_t rgb565) {
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
    if (!text || scale <= 0) return;
    int char_x = x;
    for (const char* p = text; *p; p++) {
        if (*p < 32 || *p > 126) continue;
        const uint8_t* glyph = font_5x7[*p - 32];
        for (int col = 0; col < 5; col++) {
            uint8_t column_data = glyph[col];
            int row = 0;
            while (row < 7) {
                if (!(column_data & (1 << row))) {
                    row++;
                    continue;
                }
                int run_start = row;
                while (row < 7 && (column_data & (1 << row))) row++;
                /* Collapse a contiguous "on" run into one fill_rect, same
                 * optimization boards/esp32-c6-lcd/src/gfx_mono.h uses -
                 * one real SPI window-set per run instead of per dot. */
                hal_display_fill_rect(esp32_get_display(), (int16_t)(char_x + col * scale),
                                      (int16_t)(y + run_start * scale), (uint16_t)scale,
                                      (uint16_t)((row - run_start) * scale), rgb565);
            }
        }
        char_x += (5 + 1) * scale;
    }
}

static inline int app_display_init(app_display_t* disp, const char* dev_path) {
    (void)dev_path; /* hal_display_open() ignores it too - this board has one fixed display */
    if (!disp) return -1;
    if (!esp32_get_display()) {
        return -1;
    }
    disp->width = APP_DISPLAY_WIDTH;
    disp->height = APP_DISPLAY_HEIGHT;
    disp->initialized = true;
    return 0;
}

static inline void app_display_deinit(app_display_t* disp) {
    (void)disp;
}

static inline void app_display_clear(app_display_t* disp) {
    if (disp && disp->initialized) {
        hal_display_fill_rect(esp32_get_display(), 0, 0, disp->width, disp->height, 0x0000);
    }
}

static inline void app_display_text(app_display_t* disp, int x, int y, const char* text) {
    if (disp && disp->initialized) esp32_draw_text_color(x, y, text, 1, 0xFFFF);
}

static inline void app_display_flush(app_display_t* disp) {
    /* Pixels live in the HAL framebuffer until this call. Skipping it
     * leaves the previous frame on the glass. */
    if (disp && disp->initialized) {
        hal_display_flush(esp32_get_display());
    }
}

static inline void app_display_pixel(app_display_t* disp, int x, int y, bool on) {
    if (disp && disp->initialized) {
        hal_display_draw_pixel(esp32_get_display(), (int16_t)x, (int16_t)y, on ? 0xFFFF : 0x0000);
    }
}

static inline void app_display_rect(app_display_t* disp, int x, int y, int w, int h, bool fill) {
    if (!disp || !disp->initialized || w <= 0 || h <= 0) {
        return;
    }
    if (fill) {
        hal_display_fill_rect(esp32_get_display(), (int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h,
                              0xFFFF);
    } else {
        hal_display_draw_rect(esp32_get_display(), (int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h,
                              0xFFFF);
    }
}

static inline void app_display_set_rotation(app_display_t* disp, uint8_t rot) {
    (void)disp;
    hal_display_set_rotation(esp32_get_display(), (hal_display_rotation_t)rot);
}

static inline void app_display_pixel_color(app_display_t* disp, int x, int y, uint16_t rgb565) {
    if (disp && disp->initialized) {
        hal_display_draw_pixel(esp32_get_display(), (int16_t)x, (int16_t)y, rgb565);
    }
}

/* `radius` (rounded corners) isn't supported by hal_display_fill_rect -
 * drawn as a plain rect. Visually minor; revisit if an app's design
 * depends on it looking rounded on real hardware. */
static inline void app_display_fill_rect_color(app_display_t* disp, int x, int y, int w, int h,
                                               int radius, uint16_t rgb565) {
    (void)radius;
    if (disp && disp->initialized) {
        hal_display_fill_rect(esp32_get_display(), (int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h,
                              rgb565);
    }
}

static inline void app_display_draw_rect_color(app_display_t* disp, int x, int y, int w, int h,
                                               int radius, uint16_t rgb565) {
    (void)radius;
    if (disp && disp->initialized) {
        hal_display_draw_rect(esp32_get_display(), (int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h,
                              rgb565);
    }
}

static inline void app_display_fill_circle_color(app_display_t* disp, int cx, int cy, int r,
                                                 uint16_t rgb565) {
    /* No circle primitive in hal_display.h either - approximate with a
     * bounding square fill, same "good enough for Phase 4/5" tradeoff as
     * the rect radius above. Revisit if an app's icon depends on it
     * actually looking round on real hardware. */
    if (disp && disp->initialized) {
        hal_display_fill_rect(esp32_get_display(), (int16_t)(cx - r), (int16_t)(cy - r),
                              (uint16_t)(2 * r), (uint16_t)(2 * r), rgb565);
    }
}

static inline void app_display_hline_color(app_display_t* disp, int x, int y, int w,
                                           uint16_t rgb565) {
    if (disp && disp->initialized) {
        hal_display_fill_rect(esp32_get_display(), (int16_t)x, (int16_t)y, (uint16_t)w, 1, rgb565);
    }
}

static inline void app_display_text_color(app_display_t* disp, int x, int y, const char* text,
                                          int scale, uint16_t rgb565) {
    if (disp && disp->initialized) esp32_draw_text_color(x, y, text, scale, rgb565);
}

static inline int app_display_text_width(const char* text, int scale) {
    if (!text || scale <= 0) return 0;
    int n = 0;
    for (const char* p = text; *p; p++) n++;
    return n * 6 * scale;
}

#else /* !ARDUBOT_TARGET_ESP32: sim/host build, routes to ssd1306_model */

// Simple display wrapper using the build-time panel size
// (app_display_t and app_timer_t defined in app_types.h)
static inline int app_display_init(app_display_t* disp, const char* dev_path) {
    (void)dev_path;
    if (!disp) return -1;
    ssd1306_model_register();
    disp->width = APP_DISPLAY_WIDTH;
    disp->height = APP_DISPLAY_HEIGHT;
    disp->initialized = true;
    return 0;
}

static inline void app_display_deinit(app_display_t* disp) {
    (void)disp;
}

static inline void app_display_clear(app_display_t* disp) {
    if (disp && disp->initialized) ssd1306_model_clear();
}

static inline void app_display_text(app_display_t* disp, int x, int y, const char* text) {
    if (disp && disp->initialized) ssd1306_model_draw_text(x, y, text);
}

static inline void app_display_flush(app_display_t* disp) {
    if (disp && disp->initialized) ssd1306_model_render();
}

static inline void app_display_pixel(app_display_t* disp, int x, int y, bool on) {
    if (disp && disp->initialized) {
        ssd1306_model_set_pixel(x, y, on);
    }
}

static inline void app_display_rect(app_display_t* disp, int x, int y, int w, int h, bool fill) {
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

static inline void app_display_set_rotation(app_display_t* disp, uint8_t rot) {
    (void)disp; (void)rot;
}

/* Color variants (RGB565) — for the card-style color launcher (menu.c).
 * See ssd1306_model.c for why the color API lives in a file named after a
 * mono chip. */
static inline void app_display_pixel_color(app_display_t* disp, int x, int y, uint16_t rgb565) {
    if (disp && disp->initialized) ssd1306_model_set_pixel_color(x, y, rgb565);
}

static inline void app_display_fill_rect_color(app_display_t* disp, int x, int y, int w, int h,
                                               int radius, uint16_t rgb565) {
    if (disp && disp->initialized) ssd1306_model_fill_rect_color(x, y, w, h, radius, rgb565);
}

static inline void app_display_draw_rect_color(app_display_t* disp, int x, int y, int w, int h,
                                               int radius, uint16_t rgb565) {
    if (disp && disp->initialized) ssd1306_model_draw_rect_color(x, y, w, h, radius, rgb565);
}

static inline void app_display_fill_circle_color(app_display_t* disp, int cx, int cy, int r,
                                                 uint16_t rgb565) {
    if (disp && disp->initialized) ssd1306_model_fill_circle_color(cx, cy, r, rgb565);
}

static inline void app_display_hline_color(app_display_t* disp, int x, int y, int w,
                                           uint16_t rgb565) {
    if (disp && disp->initialized) ssd1306_model_draw_hline_color(x, y, w, rgb565);
}

static inline void app_display_text_color(app_display_t* disp, int x, int y, const char* text,
                                          int scale, uint16_t rgb565) {
    if (disp && disp->initialized) ssd1306_model_draw_text_color(x, y, text, scale, rgb565);
}

static inline int app_display_text_width(const char* text, int scale) {
    return ssd1306_model_text_width(text, scale);
}

#endif /* ARDUBOT_TARGET_ESP32 */

// ============================================================================
// TIMING/FPS HELPERS
// ============================================================================

// app_timer_t defined in app_types.h
static inline void app_timer_init(app_timer_t* timer, uint32_t fps) {
    timer->target_fps = fps;
    timer->frame_time_ms = (fps > 0) ? (1000 / fps) : 0;
    timer->last_frame_ms = time_now_ms();
    timer->frame_count = 0;
    timer->delta_ms = 0;
}

static inline uint32_t app_timer_get_tick(void) {
    return time_now_ms();
}

static inline bool app_timer_should_frame(app_timer_t* timer) {
    uint32_t now = app_timer_get_tick();
    if (now - timer->last_frame_ms >= timer->frame_time_ms) {
        timer->delta_ms = now - timer->last_frame_ms;
        timer->last_frame_ms = now;
        timer->frame_count++;
        return true;
    }
    return false;
}

static inline void app_timer_sleep_remaining(app_timer_t* timer) {
    uint32_t now = app_timer_get_tick();
    uint32_t elapsed = now - timer->last_frame_ms;
    if (elapsed < timer->frame_time_ms) {
        time_sleep_ms(timer->frame_time_ms - elapsed);
    }
}

#ifdef __cplusplus
}
#endif
