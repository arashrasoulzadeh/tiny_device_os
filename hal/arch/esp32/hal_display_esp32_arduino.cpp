// hal_display_* implementation for ESP32-C6-LCD-1.47 (RISCV_TODO.md Phase 3).
//
// NOT hal_display_esp32.c (the raw ESP-IDF driver/spi_master.h stub,
// written for a theoretical espidf-framework/from-scratch-linker build
// per Phase 0's original three options - never finished, still a stub).
// Phase 0/2 settled on reusing PlatformIO's proven Arduino framework
// instead, so this implements the same hal_display.h API using
// Arduino_GFX_Library (already a lib_dep of [env:esp32-c6], already
// proven correct for this exact panel by boards/esp32-c6-lcd/src/
// gfx_mono.h / main.cpp) rather than hand-rolling ST7789 SPI timing a
// second time. Only one of the two files is ever compiled in for a given
// PlatformIO env, selected via build_src_filter.
#include "hal_display.h"
#include "display_fb.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <string.h>

// Matches device_config_esp32c6.yaml's lcd: block and gfx_mono.h's
// defaults - this board's pins are fixed, so no need to thread them
// through hal_display_config_t (which has no mosi/sclk fields anyway).
#define ESP32C6_LCD_MOSI_GPIO 6
#define ESP32C6_LCD_SCLK_GPIO 7
#define ESP32C6_LCD_CS_GPIO 14
#define ESP32C6_LCD_DC_GPIO 15
#define ESP32C6_LCD_RST_GPIO 21
#define ESP32C6_LCD_BL_GPIO 22
#define ESP32C6_LCD_NATIVE_WIDTH 172
#define ESP32C6_LCD_NATIVE_HEIGHT 320
/* Landscape size after rotation 1. One RGB565 frame is 320*172*2 = 110080
 * bytes, which fits in the C6's RAM. SPI is already 40MHz on ESP32; a full
 * frame at that rate is longer than the panel's 60Hz scan (ST7789 FRCTRL2
 * 0x0F), so writes go into RAM and hal_display_flush() sends the dirty
 * rows in one window. The glass never sees the clear underneath. */
#define ESP32C6_LCD_FB_W ESP32C6_LCD_NATIVE_HEIGHT
#define ESP32C6_LCD_FB_H ESP32C6_LCD_NATIVE_WIDTH
#define ESP32C6_LCD_SPI_HZ 40000000

struct hal_display {
    hal_display_config_t config;
    Arduino_DataBus* bus;
    Arduino_GFX* gfx;
    display_fb_t fb;
    bool initialized;
};

static uint16_t g_fb_px[ESP32C6_LCD_FB_W * ESP32C6_LCD_FB_H];

static uint16_t to_rgb565(uint32_t color, hal_display_color_format_t fmt) {
    if (fmt == HAL_DISPLAY_COLOR_RGB565) {
        return (uint16_t)color;
    }
    // RGB888/ARGB8888: top 24 bits are 0xRRGGBB - pack down to 565.
    uint8_t r = (uint8_t)(color >> 16);
    uint8_t g = (uint8_t)(color >> 8);
    uint8_t b = (uint8_t)color;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

extern "C" {

/* Real singleton, file-scope (not the header's per-translation-unit
 * static) - apps/app_framework.h's app_display_init() wrapper is a
 * `static inline` function, so every .c file that includes it (app_kit.c,
 * app_ui.c, canvas.c, each stdapp, ...) gets its OWN copy of that
 * header's "static hal_display_t* g_esp32_display" variable. Without a
 * true single source of truth down here, each of those copies would
 * independently hal_display_open()+init() its own Arduino_ESP32SPI/
 * Arduino_ST7789 pair against the same physical SPI pins - confirmed on
 * hardware (RISCV_TODO.md Phase 4) as periman "duplicate"/"No deinit
 * function" errors followed by a watchdog reset. */
static hal_display_t* g_singleton = NULL;

hal_display_t* hal_display_open(const char* path, const hal_display_config_t* config) {
    if (g_singleton) {
        return g_singleton;
    }

    hal_display_t* d = new hal_display_t();
    memset(d, 0, sizeof(*d));
    (void)path;

    if (config) {
        d->config = *config;
    } else {
        d->config.width = ESP32C6_LCD_NATIVE_HEIGHT;  // landscape: swapped
        d->config.height = ESP32C6_LCD_NATIVE_WIDTH;
        d->config.bpp = 16;
        d->config.interface = HAL_DISPLAY_INTERFACE_SPI;
        d->config.color_format = HAL_DISPLAY_COLOR_RGB565;
        d->config.rotation = HAL_DISPLAY_ROTATION_90;
        d->config.pin_dc = ESP32C6_LCD_DC_GPIO;
        d->config.pin_cs = ESP32C6_LCD_CS_GPIO;
        d->config.pin_rst = ESP32C6_LCD_RST_GPIO;
        d->config.pin_bl = ESP32C6_LCD_BL_GPIO;
    }
    g_singleton = d;
    return d;
}

void hal_display_close(hal_display_t* display) {
    if (!display || display != g_singleton) return;
    delete display->gfx;
    delete display->bus;
    delete display;
    g_singleton = NULL;
}

int hal_display_init(hal_display_t* display) {
    if (!display) return -1;
    if (display->initialized) return 0; /* already up - every caller's copy shares this one */

    display->bus = new Arduino_ESP32SPI(ESP32C6_LCD_DC_GPIO, ESP32C6_LCD_CS_GPIO,
                                         ESP32C6_LCD_SCLK_GPIO, ESP32C6_LCD_MOSI_GPIO,
                                         GFX_NOT_DEFINED);
    // rotation=1 (landscape), offsets (34,0,34,0) - proven correct for
    // this exact panel in gfx_mono.h (see its own comment for why the
    // offset pair must match, not be (0,34)).
    display->gfx = new Arduino_ST7789(display->bus, ESP32C6_LCD_RST_GPIO, 1 /*rotation*/,
                                       true /*IPS*/, ESP32C6_LCD_NATIVE_WIDTH,
                                       ESP32C6_LCD_NATIVE_HEIGHT, 34, 0, 34, 0);
    if (!display->gfx->begin(ESP32C6_LCD_SPI_HZ)) {
        return -1;
    }
    display_fb_init(&display->fb, g_fb_px, ESP32C6_LCD_FB_W, ESP32C6_LCD_FB_H);
    display_fb_fill_rect(&display->fb, 0, 0, ESP32C6_LCD_FB_W, ESP32C6_LCD_FB_H, 0x0000);
    display->gfx->draw16bitRGBBitmap(0, 0, g_fb_px, ESP32C6_LCD_FB_W, ESP32C6_LCD_FB_H);
    display_fb_clear_dirty(&display->fb);

    pinMode(ESP32C6_LCD_BL_GPIO, OUTPUT);
    digitalWrite(ESP32C6_LCD_BL_GPIO, HIGH);

    display->initialized = true;
    return 0;
}

int hal_display_deinit(hal_display_t* display) {
    if (!display || !display->initialized) return -1;
    digitalWrite(ESP32C6_LCD_BL_GPIO, LOW);
    display->initialized = false;
    return 0;
}

int hal_display_draw_bitmap(hal_display_t* display, int16_t x, int16_t y, uint16_t w, uint16_t h,
                             const uint8_t* data) {
    const uint16_t* src;
    uint16_t row;
    uint16_t col;
    if (!display || !display->initialized || !data) return -1;
    src = (const uint16_t*)data;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            display_fb_draw_pixel(&display->fb, (int16_t)(x + col), (int16_t)(y + row),
                                  src[(uint32_t)row * w + col]);
        }
    }
    return 0;
}

int hal_display_fill_rect(hal_display_t* display, int16_t x, int16_t y, uint16_t w, uint16_t h,
                           uint32_t color) {
    if (!display || !display->initialized) return -1;
    display_fb_fill_rect(&display->fb, x, y, (int16_t)w, (int16_t)h,
                         to_rgb565(color, display->config.color_format));
    return 0;
}

int hal_display_draw_pixel(hal_display_t* display, int16_t x, int16_t y, uint32_t color) {
    if (!display || !display->initialized) return -1;
    display_fb_draw_pixel(&display->fb, x, y, to_rgb565(color, display->config.color_format));
    return 0;
}

int hal_display_draw_line(hal_display_t* display, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                           uint32_t color) {
    if (!display || !display->initialized) return -1;
    display_fb_draw_line(&display->fb, x1, y1, x2, y2,
                         to_rgb565(color, display->config.color_format));
    return 0;
}

int hal_display_draw_rect(hal_display_t* display, int16_t x, int16_t y, uint16_t w, uint16_t h,
                           uint32_t color) {
    uint16_t rgb;
    if (!display || !display->initialized || w == 0 || h == 0) return -1;
    rgb = to_rgb565(color, display->config.color_format);
    display_fb_fill_rect(&display->fb, x, y, (int16_t)w, 1, rgb);
    display_fb_fill_rect(&display->fb, x, (int16_t)(y + h - 1), (int16_t)w, 1, rgb);
    display_fb_fill_rect(&display->fb, x, y, 1, (int16_t)h, rgb);
    display_fb_fill_rect(&display->fb, (int16_t)(x + w - 1), y, 1, (int16_t)h, rgb);
    return 0;
}

int hal_display_set_rotation(hal_display_t* display, hal_display_rotation_t rotation) {
    if (!display) return -1;
    display->config.rotation = rotation;
    if (display->gfx) {
        display->gfx->setRotation((uint8_t)rotation);
    }
    return 0;
}

hal_display_rotation_t hal_display_get_rotation(const hal_display_t* display) {
    return display ? (hal_display_rotation_t)display->config.rotation : HAL_DISPLAY_ROTATION_0;
}

static uint8_t g_backlight_cap = 255;

int hal_display_set_backlight_cap(uint8_t cap) {
    g_backlight_cap = cap;
    return 0;
}

int hal_display_set_brightness(hal_display_t* display, uint8_t brightness) {
    if (!display) return -1;
    if (brightness > g_backlight_cap) {
        brightness = g_backlight_cap;
    }
    // Simple on/off backlight on this board - no PWM channel wired for it.
    // A non-zero cap keeps the panel on; only a zero cap blanks it.
    digitalWrite(ESP32C6_LCD_BL_GPIO, brightness > 0 ? HIGH : LOW);
    return 0;
}

uint8_t hal_display_get_brightness(const hal_display_t* display) {
    return display && display->initialized ? 255 : 0;
}

int hal_display_sleep(hal_display_t* display) {
    if (!display) return -1;
    digitalWrite(ESP32C6_LCD_BL_GPIO, LOW);
    return 0;
}

int hal_display_wake(hal_display_t* display) {
    if (!display) return -1;
    digitalWrite(ESP32C6_LCD_BL_GPIO, HIGH);
    return 0;
}

int hal_display_flush(hal_display_t* display) {
    int16_t y = 0;
    int16_t rows = 0;
    if (!display || !display->initialized || !display->gfx) return -1;
    if (!display_fb_dirty_rows(&display->fb, &y, &rows) || rows <= 0) {
        return 0;
    }
    /* Non-const pointer selects Arduino_TFT's writePixels() path: one
     * address window for the whole dirty span, not a SPI setup per glyph. */
    display->gfx->draw16bitRGBBitmap(0, y, g_fb_px + (int32_t)y * display->fb.w, display->fb.w,
                                     rows);
    display_fb_clear_dirty(&display->fb);
    return 0;
}

int hal_display_set_flush_cb(hal_display_t* display, hal_display_flush_cb_t cb, void* arg) {
    (void)display;
    (void)cb;
    (void)arg;
    return 0;  // Panel presents from hal_display_flush(); no extra callback.
}

void hal_display_get_size(const hal_display_t* display, uint16_t* width, uint16_t* height) {
    if (!display) return;
    if (width) *width = display->config.width;
    if (height) *height = display->config.height;
}

const char* hal_display_get_path(const hal_display_t* display) {
    (void)display;
    return "esp32c6-lcd-st7789";
}

int hal_display_suspend(hal_display_t* display) { return hal_display_sleep(display); }
int hal_display_resume(hal_display_t* display) { return hal_display_wake(display); }

}  // extern "C"
