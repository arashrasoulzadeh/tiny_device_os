#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include "scheduler.h"
#include "syscall.h"
#include "app.h"
#include "vfs.h"
#include "stdlog.h"
#include "os_time.h"
#include "ardubot_keys.h"
#include "sim_gpio.h"
#include "hal_gpio.h"
#include "hal_display.h"
#include "hal_audio.h"
#include "hal_storage.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_uart.h"
#include "ssd1306_model.h"
#if !defined(ARDUBOT_PIO)
#include "bmp280_model.h"
#endif
#include "display.h"
#include "config_store.h"
#include "app_types.h"
#include "app_kit.h"
#include "app_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SIMPLIFIED MANIFEST CREATION
// ============================================================================

// Capability presets, stack/heap size presets, and app type presets now live
// in app_types.h (a dependency-free leaf header app_kit.h can include without
// pulling this umbrella header back in).

// Manifest builder function - call in app init or constructor
static inline app_manifest_t* app_manifest_create(const char* name, const char* version,
                                                  app_type_t type, uint32_t min_os_ver,
                                                  void (*entry)(void),
                                                  uint32_t stack_size, uint32_t heap_size,
                                                  const capability_t* caps, uint32_t cap_count,
                                                  const char* author, const char* description) {
    static app_manifest_t manifest = {0};
    manifest.type = type;
    manifest.min_os_version = min_os_ver;
    manifest.entry_point = (uintptr_t)entry;
    manifest.stack_size = stack_size > 0 ? stack_size : APP_STACK_SMALL;
    manifest.heap_size = heap_size > 0 ? heap_size : APP_HEAP_SMALL;
    
    if (name) strncpy(manifest.name, name, APP_NAME_MAX - 1);
    if (version) strncpy(manifest.version, version, 15);
    if (author) strncpy(manifest.author, author, 63);
    if (description) strncpy(manifest.description, description, 255);
    
    if (caps && cap_count > 0) {
        manifest.capability_count = (cap_count < 16) ? cap_count : 16;
        for (uint32_t i = 0; i < manifest.capability_count; i++) {
            manifest.capabilities[i] = caps[i];
        }
    }
    
    return &manifest;
}

// ============================================================================
// APP LIFECYCLE HELPERS
// ============================================================================

// App state callbacks
typedef struct {
    void (*on_init)(void);           // Called once at startup
    void (*on_start)(void);          // Called when app becomes active
    void (*on_stop)(void);           // Called when app stops
    void (*on_suspend)(void);        // Called when app suspended
    void (*on_resume)(void);         // Called when app resumes
    void (*on_loop)(void);           // Called every frame
    void (*on_render)(void);         // Called for rendering
    void (*on_event)(uint32_t event, void* data); // System events
    void (*on_cleanup)(void);        // Called on app exit
} app_lifecycle_t;

// Global lifecycle (set by app)
extern app_lifecycle_t g_app_lifecycle;

// Run app with lifecycle
int app_run_with_lifecycle(const app_lifecycle_t* lifecycle);

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
    (void)disp; /* Arduino_GFX writes straight to the panel - nothing to flush. */
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
// BUTTON/INPUT HELPERS
// ============================================================================

// Button configuration
typedef struct {
    int pin;
    sim_key_t key;
    hal_gpio_irq_t trigger;
    void (*on_press)(int pin, void* arg);
    void (*on_release)(int pin, void* arg);
    void* arg;
} app_button_t;

static inline int app_button_init(const app_button_t* btn) {
    if (!btn) return -1;

    // Register sim GPIO
    sim_gpio_register(btn->pin, false);

#if !defined(ARDUBOT_TARGET_ESP32)
    /* btn->pin is a virtual slot index (apps/app_kit.c's g_next_pin,
     * starting at APP_KIT_PIN_BASE=20), meaningful only as a key into
     * sim_gpio's own in-memory table below - it has no relationship to
     * a real GPIO pin number. hal_gpio_open()/set_irq() here always
     * passed a NULL callback anyway (see below), so this path was
     * already functionally inert even on sim/host builds - it's real
     * hardware's gpio_config() that can't tell "virtual slot 21" from
     * "real GPIO21" (this board's ST7789 reset pin), which hung/faulted
     * by reconfiguring a pin another driver already owns (confirmed on
     * hardware, RISCV_TODO.md Phase 4). Skipped entirely on ESP32 - the
     * real key-press path is sim_gpio_handle_key(), driven by actual
     * button polling in kernel_boot.cpp, not this. */
    char path[32];
    snprintf(path, sizeof(path), "/dev/gpio%d", btn->pin);
    hal_gpio_t* gpio = hal_gpio_open(path, HAL_GPIO_MODE_INPUT_PULLUP);
    if (!gpio) return -1;
#endif

    // Map key to pin
    sim_gpio_set_key_mapping(btn->key, btn->pin, true);

    // Register callback with proper trigger
    if (btn->on_press || btn->on_release) {
        sim_gpio_register_hal_gpio_with_trigger(btn->pin,
            (hal_gpio_callback_t)(btn->trigger == HAL_GPIO_IRQ_FALLING ? btn->on_release : btn->on_press),
            btn->arg, btn->trigger);
    }

#if !defined(ARDUBOT_TARGET_ESP32)
    hal_gpio_set_irq(gpio, btn->trigger, NULL, NULL);
    hal_gpio_enable_irq(gpio);
#endif

    return 0;
}

// Multi-button helper
typedef struct {
    app_button_t* buttons;
    uint32_t count;
} app_buttons_t;

static inline int app_buttons_init(app_buttons_t* btns) {
    if (!btns || !btns->buttons) return -1;
    for (uint32_t i = 0; i < btns->count; i++) {
        if (app_button_init(&btns->buttons[i]) != 0) return -1;
    }
    return 0;
}

// Quick button setup macros
#define APP_BUTTON(pin_num, key_code, press_fn, release_fn, user_arg) \
    { .pin = pin_num, .key = key_code, .trigger = HAL_GPIO_IRQ_RISING, \
      .on_press = press_fn, .on_release = release_fn, .arg = user_arg }

// Used by app_ui_bind_key()'s trampoline, which can't tell press from
// release apart - HAL_GPIO_IRQ_BOTH fired it on both edges of one tap,
// doubling every nav/select action (e.g. one arrow press moving the
// selection twice). Press-only, matching APP_BUTTON above.
#define APP_KEY_BUTTON(pin_num, key_code, press_fn, release_fn, user_arg) \
    { .pin = pin_num, .key = key_code, .trigger = HAL_GPIO_IRQ_RISING, \
      .on_press = press_fn, .on_release = release_fn, .arg = user_arg }

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

// ============================================================================
// LOGGING HELPERS
// ============================================================================

// Internal logging function using stdlog_vlog correctly
static inline void _app_log(log_level_t level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    stdlog_vlog(level, __FILE__, __LINE__, __func__, fmt, args);
    va_end(args);
}

/* `(fmt, ...)` + `##__VA_ARGS__` (GNU's comma-swallowing extension) let a
 * caller pass just a literal string with no extra format args - but this
 * project builds with CMAKE_C_EXTENSIONS OFF (strict -std=c11), where
 * ISO C requires at least one argument for `...` and `##__VA_ARGS__`
 * itself isn't standard either; gcc 13 flags both under -Werror. Folding
 * the whole call into a single `...` avoids a separately-required `fmt`
 * parameter entirely - `"[APP] "` and the format string are still
 * adjacent string-literal tokens after substitution, so the compiler's
 * ordinary string-literal concatenation still joins them exactly as
 * before (e.g. APP_INFO("x") -> _app_log(..., "[APP] " "x"); APP_INFO("n=%d", n)
 * -> _app_log(..., "[APP] " "n=%d", n)). */

// Use standard logging with app name prefix
#define APP_LOG_DEBUG(...)   _app_log(LOG_LEVEL_DEBUG, "[APP] " __VA_ARGS__)
#define APP_LOG_INFO(...)    _app_log(LOG_LEVEL_INFO, "[APP] " __VA_ARGS__)
#define APP_LOG_WARN(...)    _app_log(LOG_LEVEL_WARN, "[APP] " __VA_ARGS__)
#define APP_LOG_ERROR(...)   _app_log(LOG_LEVEL_ERROR, "[APP] " __VA_ARGS__)

// Short aliases
#define APP_DEBUG(...)       _app_log(LOG_LEVEL_DEBUG, "[APP] " __VA_ARGS__)
#define APP_INFO(...)        _app_log(LOG_LEVEL_INFO, "[APP] " __VA_ARGS__)
#define APP_WARN(...)        _app_log(LOG_LEVEL_WARN, "[APP] " __VA_ARGS__)
#define APP_ERROR(...)       _app_log(LOG_LEVEL_ERROR, "[APP] " __VA_ARGS__)

// ============================================================================
// CONFIG/SETTINGS HELPERS - global store (for built-in settings)
// ============================================================================

static config_store_t* g_config_store = NULL;

static inline int app_config_set_str(const char* key, const char* value) {
    if (!g_config_store) return -1;
    return config_set_string(g_config_store, key, value);
}

static inline const char* app_config_get_str(const char* key, const char* def) {
    if (!g_config_store) return def;
    return config_get_string(g_config_store, key, def);
}

static inline int app_config_set_int(const char* key, int32_t value) {
    if (!g_config_store) return -1;
    return config_set_int(g_config_store, key, value);
}

static inline int app_config_get_int(const char* key, int32_t def) {
    if (!g_config_store) return def;
    return config_get_int(g_config_store, key, def);
}

static inline int app_config_set_bool(const char* key, bool value) {
    if (!g_config_store) return -1;
    return config_set_bool(g_config_store, key, value);
}

static inline bool app_config_get_bool(const char* key, bool def) {
    if (!g_config_store) return def;
    return config_get_bool(g_config_store, key, def);
}

static inline void app_config_init(config_store_t* store) {
    g_config_store = store;
}

static inline void app_config_save(void) {
    if (g_config_store) config_flush(g_config_store);
}

// ============================================================================
// STORAGE/FILE HELPERS
// ============================================================================

typedef vfs_file_t app_file_t;

static inline int app_file_open(app_file_t** file, const char* path, const char* mode) {
    vfs_mode_t vfs_mode = 0;
    if (strchr(mode, 'r')) vfs_mode |= VFS_MODE_READ;
    if (strchr(mode, 'w')) vfs_mode |= VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC;
    if (strchr(mode, 'a')) vfs_mode |= VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_APPEND;
    if (strchr(mode, '+')) vfs_mode |= VFS_MODE_READ | VFS_MODE_WRITE;
    return vfs_open(path, vfs_mode, file);
}

static inline int app_file_close(app_file_t* file) {
    return vfs_close(file);
}

static inline ssize_t app_file_read(app_file_t* file, void* buf, size_t count) {
    return vfs_read(file, buf, count);
}

static inline ssize_t app_file_write(app_file_t* file, const void* buf, size_t count) {
    return vfs_write(file, buf, count);
}

static inline int app_file_puts(app_file_t* file, const char* str) {
    return vfs_write(file, str, strlen(str));
}

static inline int app_file_gets(app_file_t* file, char* buf, size_t max) {
    size_t i = 0;
    char c;
    while (i < max - 1 && vfs_read(file, &c, 1) == 1) {
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (i > 0) ? (int)i : -1;
}

static inline bool app_file_exists(const char* path) {
    vfs_stat_t st;
    return vfs_stat(path, &st) == 0;
}

static inline int app_file_remove(const char* path) {
    return vfs_unlink(path);
}

static inline int app_dir_create(const char* path) {
    return vfs_mkdir(path, 0755);
}

// ============================================================================
// AUDIO HELPERS
// ============================================================================

typedef struct {
    hal_audio_t* handle;
    uint32_t sample_rate;
    uint16_t channels;
} app_audio_t;

static inline int app_audio_init(app_audio_t* audio, uint32_t sample_rate, uint16_t channels) {
    if (!audio) return -1;
    hal_audio_config_t cfg = {
        .sample_rate = sample_rate,
        .channels = channels,
        .format = HAL_AUDIO_FORMAT_PCM_S16_LE,
        .buffer_frames = 512,
        .period_frames = 256,
        .output = true,
        .input = false
    };
    audio->handle = hal_audio_open("/dev/audio0", &cfg);
    if (!audio->handle) return -1;
    audio->sample_rate = sample_rate;
    audio->channels = channels;
    return hal_audio_start(audio->handle);
}

static inline void app_audio_deinit(app_audio_t* audio) {
    if (audio && audio->handle) {
        hal_audio_stop(audio->handle);
        hal_audio_close(audio->handle);
        audio->handle = NULL;
    }
}

static inline int app_audio_play(app_audio_t* audio, const int16_t* samples, size_t count) {
    if (!audio || !audio->handle || !samples) return -1;
    return hal_audio_write(audio->handle, samples, count);
}

// ============================================================================
// UI Framework - see app_ui.h for full definitions

// ============================================================================
// SIMPLE APP TEMPLATE
// ============================================================================

// Simple app entry point template
// Usage: 
//   app_manifest_t* my_manifest = app_manifest_create(...);
//   int app_main(void) { ... init ... while(1) { ... loop ... } }
#define APP_ENTRY_POINT(name) \
    app_manifest_t* name##_manifest = NULL; \
    __attribute__((constructor)) \
    static void name##_manifest_init(void) { \
        extern app_manifest_t* app_manifest_create(...); /* placeholder */ \
    } \
    int name##_entry(void)

#ifdef __cplusplus
}
#endif