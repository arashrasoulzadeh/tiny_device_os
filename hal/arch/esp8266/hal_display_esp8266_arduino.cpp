// hal_display_* implementation for NodeMCU (ESP8266) with I2C SSD1306/SH1106.
// Uses the local ssd1306_mini.h driver (boards/nodemcu/src/ssd1306_mini.h)
// rather than an external library, to avoid registry dependency.
#include "hal_display.h"
#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>
#include <string.h>

#include "ssd1306_mini.h"

#if defined(__has_include)
#if __has_include("device_config.h")
#include "device_config.h"
#endif
#endif

/* device_config.h (from the active YAML) is the panel. The rotator profile
 * is a 128x64 SH1106; SSD1306 horizontal addressing on that glass only
 * writes page 0, so the top band goes black and the rest stays noise. */
#ifndef ARDUBOT_LCD_I2C_ADDR
#define ARDUBOT_LCD_I2C_ADDR 0x3C
#endif
#ifndef ARDUBOT_LCD_SCL_GPIO
#define ARDUBOT_LCD_SCL_GPIO 5
#endif
#ifndef ARDUBOT_LCD_SDA_GPIO
#define ARDUBOT_LCD_SDA_GPIO 4
#endif
#ifndef ARDUBOT_LCD_WIDTH
#define ARDUBOT_LCD_WIDTH 128
#endif
#ifndef ARDUBOT_LCD_HEIGHT
#define ARDUBOT_LCD_HEIGHT 32
#endif
#ifndef ARDUBOT_LCD_IS_SH1106
#define ARDUBOT_LCD_IS_SH1106 0
#endif
#ifndef ARDUBOT_LCD_COL_OFFSET
#define ARDUBOT_LCD_COL_OFFSET 2
#endif

struct hal_display {
  hal_display_config_t config;
  Ssd1306 *ssd1306;
  bool initialized;
};

static hal_display_t *g_singleton = NULL;

extern "C" {

hal_display_t *hal_display_open(const char *path,
                                const hal_display_config_t *config) {
  if (g_singleton) {
    return g_singleton;
  }

  hal_display_t *d = new hal_display_t();
  memset(d, 0, sizeof(*d));
  (void)path;

  if (config) {
    d->config = *config;
  } else {
    d->config.width = ARDUBOT_LCD_WIDTH;
    d->config.height = ARDUBOT_LCD_HEIGHT;
    d->config.bpp = 1;
    d->config.interface = HAL_DISPLAY_INTERFACE_I2C;
    d->config.color_format = HAL_DISPLAY_COLOR_MONO;
    d->config.rotation = HAL_DISPLAY_ROTATION_0;
    d->config.pin_dc = ARDUBOT_LCD_SCL_GPIO;
    d->config.pin_cs = ARDUBOT_LCD_SDA_GPIO;
    d->config.pin_rst = -1;
    d->config.pin_bl = -1;
  }
  g_singleton = d;
  return d;
}

void hal_display_close(hal_display_t *display) {
  if (!display || display != g_singleton)
    return;
  delete display->ssd1306;
  delete display;
  g_singleton = NULL;
}

int hal_display_init(hal_display_t *display) {
  if (!display) {
    return -1;
  }
  if (display->initialized)
    return 0;

  int scl_gpio = (display->config.pin_dc >= 0) ? display->config.pin_dc
                                               : ARDUBOT_LCD_SCL_GPIO;
  int sda_gpio = (display->config.pin_cs >= 0) ? display->config.pin_cs
                                               : ARDUBOT_LCD_SDA_GPIO;

  Wire.begin(sda_gpio, scl_gpio);
  Wire.setClock(100000); // 100kHz more reliable for SSD1306
  delay(10);             // Let bus settle

  display->ssd1306 = new Ssd1306(
      (uint8_t)display->config.width, (uint8_t)display->config.height,
      (uint8_t)ARDUBOT_LCD_I2C_ADDR, ARDUBOT_LCD_IS_SH1106 != 0,
      (uint8_t)ARDUBOT_LCD_COL_OFFSET);
  if (!display->ssd1306) {
    return -1;
  }
  if (!display->ssd1306->begin()) {
    delete display->ssd1306;
    display->ssd1306 = NULL;
    return -1;
  }

  display->initialized = true;
  return 0;
}

int hal_display_deinit(hal_display_t *display) {
  if (!display || !display->initialized)
    return -1;
  if (display->ssd1306) {
    display->ssd1306->clear();
    display->ssd1306->display();
  }
  display->initialized = false;
  return 0;
}

/* This 128x64 panel has one bit per pixel. RGB565 still has to survive:
 * a dark title bar or track must stay off, or the bright text drawn on
 * it disappears. Threshold is above the dark chrome and below text. */
static inline uint8_t to_mono(uint32_t color) {
  uint16_t c = (uint16_t)color;
  unsigned r = ((c >> 11) & 0x1Fu) * 255u / 31u;
  unsigned g = ((c >> 5) & 0x3Fu) * 255u / 63u;
  unsigned b = (c & 0x1Fu) * 255u / 31u;
  unsigned luma = (r * 30u + g * 59u + b * 11u) / 100u;
  return luma >= 48u ? 1u : 0u;
}

int hal_display_draw_bitmap(hal_display_t *display, int16_t x, int16_t y,
                            uint16_t w, uint16_t h, const uint8_t *data) {
  if (!display || !display->initialized || !display->ssd1306 || !data)
    return -1;
  // The Ssd1306 class uses set_pixel for individual pixels
  // For bitmap, we'd need to write directly to its framebuffer
  // For now, just draw pixel by pixel (could be optimized)
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      size_t byte_idx = (row * w + col) / 8;
      uint8_t bit = (data[byte_idx] >> (col % 8)) & 1;
      if (bit) {
        display->ssd1306->set_pixel(x + col, y + row, true);
      }
    }
  }
  return 0;
}

int hal_display_fill_rect(hal_display_t *display, int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint32_t color) {
  if (!display || !display->initialized || !display->ssd1306)
    return -1;
  uint8_t mono = to_mono(color);
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      display->ssd1306->set_pixel(x + col, y + row, mono);
    }
  }
  return 0;
}

int hal_display_draw_pixel(hal_display_t *display, int16_t x, int16_t y,
                           uint32_t color) {
  if (!display || !display->initialized || !display->ssd1306)
    return -1;
  display->ssd1306->set_pixel(x, y, to_mono(color));
  return 0;
}

int hal_display_draw_line(hal_display_t *display, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint32_t color) {
  if (!display || !display->initialized || !display->ssd1306)
    return -1;
  uint8_t mono = to_mono(color);
  // Bresenham's line algorithm
  int16_t dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
  int16_t dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
  int16_t err = dx + dy, e2;

  while (1) {
    display->ssd1306->set_pixel(x1, y1, mono);
    if (x1 == x2 && y1 == y2)
      break;
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x1 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y1 += sy;
    }
  }
  return 0;
}

int hal_display_draw_rect(hal_display_t *display, int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint32_t color) {
  if (!display || !display->initialized || !display->ssd1306 || w == 0 ||
      h == 0)
    return -1;
  uint8_t mono = to_mono(color);
  for (uint16_t col = 0; col < w; col++) {
    display->ssd1306->set_pixel(x + col, y, mono);
    display->ssd1306->set_pixel(x + col, y + h - 1, mono);
  }
  for (uint16_t row = 0; row < h; row++) {
    display->ssd1306->set_pixel(x, y + row, mono);
    display->ssd1306->set_pixel(x + w - 1, y + row, mono);
  }
  return 0;
}

int hal_display_set_rotation(hal_display_t *display,
                             hal_display_rotation_t rotation) {
  if (!display)
    return -1;
  display->config.rotation = rotation;
  // SSD1306 rotation would require remapping - not implemented for now
  return 0;
}

hal_display_rotation_t hal_display_get_rotation(const hal_display_t *display) {
  return display ? (hal_display_rotation_t)display->config.rotation
                 : HAL_DISPLAY_ROTATION_0;
}

static uint8_t g_backlight_cap = 255;

int hal_display_set_backlight_cap(uint8_t cap) {
  g_backlight_cap = cap;
  return 0;
}

int hal_display_set_brightness(hal_display_t *display, uint8_t brightness) {
  if (!display)
    return -1;
  if (brightness > g_backlight_cap) {
    brightness = g_backlight_cap;
  }
  // SSD1306 doesn't typically have hardware brightness control
  // Could implement contrast control via SSD1306_CMD_SET_CONTRAST if needed
  (void)brightness;
  return 0;
}

uint8_t hal_display_get_brightness(const hal_display_t *display) {
  return display && display->initialized ? 255 : 0;
}

int hal_display_sleep(hal_display_t *display) {
  if (!display)
    return -1;
  if (display->ssd1306) {
    Wire.beginTransmission(ARDUBOT_LCD_I2C_ADDR);
    Wire.write(0x00);
    Wire.write(0xAE); // Display off
    Wire.endTransmission();
  }
  return 0;
}

int hal_display_wake(hal_display_t *display) {
  if (!display)
    return -1;
  if (display->ssd1306) {
    Wire.beginTransmission(ARDUBOT_LCD_I2C_ADDR);
    Wire.write(0x00);
    Wire.write(0xAF); // Display on
    Wire.endTransmission();
  }
  return 0;
}

int hal_display_flush(hal_display_t *display) {
  if (!display || !display->initialized || !display->ssd1306)
    return -1;
  // Ssd1306.display() pushes only changed pages to the display
  display->ssd1306->display();
  return 0;
}

int hal_display_set_flush_cb(hal_display_t *display, hal_display_flush_cb_t cb,
                             void *arg) {
  (void)display;
  (void)cb;
  (void)arg;
  return 0;
}

void hal_display_get_size(const hal_display_t *display, uint16_t *width,
                          uint16_t *height) {
  if (!display)
    return;
  if (width)
    *width = display->config.width;
  if (height)
    *height = display->config.height;
}

const char *hal_display_get_path(const hal_display_t *display) {
  (void)display;
  return "nodemcu-ssd1306-i2c";
}

int hal_display_suspend(hal_display_t *display) {
  return hal_display_sleep(display);
}
int hal_display_resume(hal_display_t *display) {
  return hal_display_wake(display);
}

} // extern "C"
