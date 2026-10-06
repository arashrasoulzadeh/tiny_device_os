/**
 * Drop-in replacement for boards/nodemcu/src/ssd1306_mini.h's `Ssd1306` API,
 * backed by Arduino_GFX driving this board's ST7789 SPI color panel instead
 * of an I2C monochrome OLED — landscape orientation, real RGB565 color.
 *
 * Keeps the same call surface (begin/clear/set_pixel/draw_text/display)
 * as boards/nodemcu/src/ssd1306_mini.h, and adds set_fg_color()/set_bg_color()
 * so a caller can pick a color before each draw. Apps do not use this
 * header: device apps are apps/stdapps/, drawn through hal_display.
 * Pixels are drawn
 * immediately (no 1bpp framebuffer) so each set_pixel() can use whatever
 * fg/bg color was most recently set. The logical WxH canvas is centered on
 * the physical panel, landscape 320x172.
 */

#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <stdint.h>
#include <string.h>

#include "device_config.h"

/* Panel is 172x320 in its native (portrait) orientation; constructed at
 * rotation=1 below for landscape, where Arduino_GFX reports w()/h() as
 * 320x172 and swaps the column/row offset pair accordingly. */
#define GFXMONO_PANEL_NATIVE_WIDTH 172
#define GFXMONO_PANEL_NATIVE_HEIGHT 320

/* Pins come from device_config_esp32c6.yaml's lcd.mosi/sclk/cs/dc/rst/bl via
 * scripts/device_config.py — fall back to this board's known-good wiring if
 * a profile omits one (e.g. an older device_config_esp32c6.yaml). */
#ifndef ARDUBOT_LCD_MOSI_GPIO
#define ARDUBOT_LCD_MOSI_GPIO 6
#endif
#ifndef ARDUBOT_LCD_SCLK_GPIO
#define ARDUBOT_LCD_SCLK_GPIO 7
#endif
#ifndef ARDUBOT_LCD_CS_GPIO
#define ARDUBOT_LCD_CS_GPIO 14
#endif
#ifndef ARDUBOT_LCD_DC_GPIO
#define ARDUBOT_LCD_DC_GPIO 15
#endif
#ifndef ARDUBOT_LCD_RST_GPIO
#define ARDUBOT_LCD_RST_GPIO 21
#endif
#ifndef ARDUBOT_LCD_BL_GPIO
#define ARDUBOT_LCD_BL_GPIO 22
#endif
/* How many physical pixels each logical pixel draws as — bumps up text/icon
 * size on this high-res panel without redesigning the 5x7 font or 16x16
 * icon format. 1 logical pixel : 1 physical pixel looked tiny on a 320x172
 * screen (the font/icons were designed for a 128x64-ish OLED). */
#ifndef ARDUBOT_LCD_SCALE
#define ARDUBOT_LCD_SCALE 2
#endif
/* Margin (physical pixels) kept clear between the canvas and the panel's
 * edges/corners — the static border is painted once in begin() and never
 * touched again by clear()/set_pixel(), which only ever draw inside it. */
#ifndef ARDUBOT_LCD_PADDING
#define ARDUBOT_LCD_PADDING 10
#endif

class GfxMono {
 public:
  GfxMono(uint16_t width, uint16_t height, uint8_t /*addr*/ = 0,
          bool /*sh1106*/ = false, uint8_t /*col_offset*/ = 0)
      : width_(width), height_(height) {}

  bool begin() {
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);
    if (!gfx_->begin()) {
      return false;
    }
    off_x_ = PADDING + ((gfx_->width() - 2 * PADDING) - width_ * SCALE) / 2;
    off_y_ = PADDING + ((gfx_->height() - 2 * PADDING) - height_ * SCALE) / 2;
    gfx_->fillScreen(bg_color_);
    return true;
  }

  void set_bg_color(uint16_t c) { bg_color_ = c; }
  void set_fg_color(uint16_t c) { fg_color_ = c; }

  void clear() {
    gfx_->fillRect(off_x_, off_y_, width_ * SCALE, height_ * SCALE, bg_color_);
  }

  void set_pixel(int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
      return;
    }
    gfx_->fillRect(off_x_ + x * SCALE, off_y_ + y * SCALE, SCALE, SCALE,
                   on ? fg_color_ : bg_color_);
  }

  /* tscale blows up each glyph pixel into a tscale x tscale block (on top
   * of the panel's own SCALE) — for a title/headline that needs to read
   * from further away than the base 5x7 font allows at 1x.
   *
   * Each "on" pixel used to be its own set_pixel() call, i.e. its own SPI
   * command sequence (CASET/RASET/RAMWR) — a scale-2 character can have
   * 50+ "on" sub-pixels, so one string redraw was hundreds of tiny SPI
   * transactions, slow enough to visibly "paint in" rather than appear
   * instantly (reported as flicker on screens that redraw a line every
   * second). Collapsing each column's contiguous run of set bits into one
   * fill_box() call cuts that by roughly the average run length. */
  void draw_char(int x, int y, char c, int tscale = 1) {
    if (c < 32 || c > 127) {
      c = '?';
    }
    const uint8_t* g = glyph((uint8_t)c);
    for (int col = 0; col < 5; col++) {
      uint8_t bits = g[col];
      int row = 0;
      while (row < 7) {
        if (!(bits & (1u << row))) {
          row++;
          continue;
        }
        int run_start = row;
        while (row < 7 && (bits & (1u << row))) {
          row++;
        }
        int run_len = row - run_start;
        fill_box(x + col * tscale, y + run_start * tscale, tscale, run_len * tscale, 0, fg_color_);
      }
    }
  }

  void draw_text(int x, int y, const char* s, int tscale = 1) {
    while (*s) {
      draw_char(x, y, *s++, tscale);
      x += 6 * tscale;
    }
  }

  int text_width(const char* s, int tscale = 1) const { return (int)strlen(s) * 6 * tscale; }

  /* Pixels already hit the panel in set_pixel()/clear() — nothing to flush.
   * Kept so call sites written against Ssd1306's buffered API need no
   * changes. */
  void display() {}

  /* Colored glow-style frame around a logical-space box (icon selection
   * highlight) — draws two nested rounded rects directly via Arduino_GFX
   * rather than through set_pixel()'s 1bpp-style bitmap path, so it can use
   * its own color independent of whatever fg_color_ the caller last set. */
  void draw_glow_frame(int x, int y, int w, int h, uint16_t color) {
    const int px = off_x_ + x * SCALE;
    const int py = off_y_ + y * SCALE;
    const int pw = w * SCALE;
    const int ph = h * SCALE;
    gfx_->drawRoundRect(px - 2, py - 2, pw + 4, ph + 4, 3, color);
    gfx_->drawRoundRect(px - 1, py - 1, pw + 2, ph + 2, 3, color);
  }

  /* Card/tile building blocks — logical-space rounded rect fill/outline and
   * a horizontal rule, all drawn directly (bypass set_pixel()'s bitmap
   * path) so the launcher's status bar / app cards / hint bar can use
   * colors independent of fg_color_. */
  void fill_box(int x, int y, int w, int h, int radius, uint16_t color) {
    gfx_->fillRoundRect(off_x_ + x * SCALE, off_y_ + y * SCALE, w * SCALE, h * SCALE, radius,
                        color);
  }

  void draw_box(int x, int y, int w, int h, int radius, uint16_t color) {
    gfx_->drawRoundRect(off_x_ + x * SCALE, off_y_ + y * SCALE, w * SCALE, h * SCALE, radius,
                        color);
  }

  void fill_circle(int x, int y, int r, uint16_t color) {
    gfx_->fillCircle(off_x_ + x * SCALE, off_y_ + y * SCALE, r * SCALE, color);
  }

  void draw_hline(int x, int y, int w, uint16_t color) {
    gfx_->fillRect(off_x_ + x * SCALE, off_y_ + y * SCALE, w * SCALE, SCALE, color);
  }

 private:
  static const uint8_t* glyph(uint8_t c);

  static constexpr int SCALE = ARDUBOT_LCD_SCALE;
  static constexpr int PADDING = ARDUBOT_LCD_PADDING;
  static constexpr int LCD_MOSI = ARDUBOT_LCD_MOSI_GPIO;
  static constexpr int LCD_SCLK = ARDUBOT_LCD_SCLK_GPIO;
  static constexpr int LCD_CS = ARDUBOT_LCD_CS_GPIO;
  static constexpr int LCD_DC = ARDUBOT_LCD_DC_GPIO;
  static constexpr int LCD_RST = ARDUBOT_LCD_RST_GPIO;
  static constexpr int LCD_BL = ARDUBOT_LCD_BL_GPIO;

  Arduino_DataBus* bus_ = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED);
  /* rotation=1 (landscape): Arduino_TFT::setRotation() computes
   * xStart=ROW_OFFSET1, yStart=COL_OFFSET2 for this case — for this
   * symmetric 172x320 panel that means pair 2 must equal pair 1 (34,0),
   * not (0,34) (that was the earlier bug: it left yStart=0, squeezing the
   * canvas into the top of the panel instead of centering it). */
  Arduino_GFX* gfx_ = new Arduino_ST7789(bus_, LCD_RST, 1 /*rotation*/, true /*IPS*/,
                                         GFXMONO_PANEL_NATIVE_WIDTH, GFXMONO_PANEL_NATIVE_HEIGHT,
                                         34, 0, 34, 0);

  uint16_t width_;
  uint16_t height_;
  int off_x_ = 0;
  int off_y_ = 0;
  uint16_t bg_color_ = RGB565_BLACK;
  uint16_t fg_color_ = RGB565_WHITE;
};

/* 5x7 font, ASCII 32..127 subset — copied from ssd1306_mini.h's glyph table
 * so this adapter has no dependency on the I2C driver it replaces. */
inline const uint8_t* GfxMono::glyph(uint8_t c) {
  static const uint8_t font[96][5] = {
      {0x00, 0x00, 0x00, 0x00, 0x00},  // sp
      {0x00, 0x00, 0x5F, 0x00, 0x00},  // !
      {0x00, 0x07, 0x00, 0x07, 0x00},  // "
      {0x14, 0x7F, 0x14, 0x7F, 0x14},  // #
      {0x24, 0x2A, 0x7F, 0x2A, 0x12},  // $
      {0x23, 0x13, 0x08, 0x64, 0x62},  // %
      {0x36, 0x49, 0x55, 0x22, 0x50},  // &
      {0x00, 0x05, 0x03, 0x00, 0x00},  // '
      {0x00, 0x1C, 0x22, 0x41, 0x00},  // (
      {0x00, 0x41, 0x22, 0x1C, 0x00},  // )
      {0x08, 0x2A, 0x1C, 0x2A, 0x08},  // *
      {0x08, 0x08, 0x3E, 0x08, 0x08},  // +
      {0x00, 0x50, 0x30, 0x00, 0x00},  // ,
      {0x08, 0x08, 0x08, 0x08, 0x08},  // -
      {0x00, 0x60, 0x60, 0x00, 0x00},  // .
      {0x20, 0x10, 0x08, 0x04, 0x02},  // /
      {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
      {0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
      {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
      {0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
      {0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
      {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
      {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
      {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
      {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
      {0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
      {0x00, 0x36, 0x36, 0x00, 0x00},  // :
      {0x00, 0x56, 0x36, 0x00, 0x00},  // ;
      {0x00, 0x08, 0x14, 0x22, 0x41},  // <
      {0x14, 0x14, 0x14, 0x14, 0x14},  // =
      {0x41, 0x22, 0x14, 0x08, 0x00},  // >
      {0x02, 0x01, 0x51, 0x09, 0x06},  // ?
      {0x32, 0x49, 0x79, 0x41, 0x3E},  // @
      {0x7E, 0x11, 0x11, 0x11, 0x7E},  // A
      {0x7F, 0x49, 0x49, 0x49, 0x36},  // B
      {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
      {0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
      {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
      {0x7F, 0x09, 0x09, 0x01, 0x01},  // F
      {0x3E, 0x41, 0x41, 0x51, 0x32},  // G
      {0x7F, 0x08, 0x08, 0x08, 0x7F},  // H
      {0x00, 0x41, 0x7F, 0x41, 0x00},  // I
      {0x20, 0x40, 0x41, 0x3F, 0x01},  // J
      {0x7F, 0x08, 0x14, 0x22, 0x41},  // K
      {0x7F, 0x40, 0x40, 0x40, 0x40},  // L
      {0x7F, 0x02, 0x04, 0x02, 0x7F},  // M
      {0x7F, 0x04, 0x08, 0x10, 0x7F},  // N
      {0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
      {0x7F, 0x09, 0x09, 0x09, 0x06},  // P
      {0x3E, 0x41, 0x51, 0x21, 0x5E},  // Q
      {0x7F, 0x09, 0x19, 0x29, 0x46},  // R
      {0x46, 0x49, 0x49, 0x49, 0x31},  // S
      {0x01, 0x01, 0x7F, 0x01, 0x01},  // T
      {0x3F, 0x40, 0x40, 0x40, 0x3F},  // U
      {0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
      {0x7F, 0x20, 0x18, 0x20, 0x7F},  // W
      {0x63, 0x14, 0x08, 0x14, 0x63},  // X
      {0x03, 0x04, 0x78, 0x04, 0x03},  // Y
      {0x61, 0x51, 0x49, 0x45, 0x43},  // Z
      {0x00, 0x00, 0x7F, 0x41, 0x41},  // [
      {0x02, 0x04, 0x08, 0x10, 0x20},  // backslash
      {0x41, 0x41, 0x7F, 0x00, 0x00},  // ]
      {0x04, 0x02, 0x01, 0x02, 0x04},  // ^
      {0x40, 0x40, 0x40, 0x40, 0x40},  // _
      {0x00, 0x01, 0x02, 0x04, 0x00},  // `
      {0x20, 0x54, 0x54, 0x54, 0x78},  // a
      {0x7F, 0x48, 0x44, 0x44, 0x38},  // b
      {0x38, 0x44, 0x44, 0x44, 0x20},  // c
      {0x38, 0x44, 0x44, 0x48, 0x7F},  // d
      {0x38, 0x54, 0x54, 0x54, 0x18},  // e
      {0x08, 0x7E, 0x09, 0x01, 0x02},  // f
      {0x08, 0x14, 0x54, 0x54, 0x3C},  // g
      {0x7F, 0x08, 0x04, 0x04, 0x78},  // h
      {0x00, 0x44, 0x7D, 0x40, 0x00},  // i
      {0x20, 0x40, 0x44, 0x3D, 0x00},  // j
      {0x00, 0x7F, 0x10, 0x28, 0x44},  // k
      {0x00, 0x41, 0x7F, 0x40, 0x00},  // l
      {0x7C, 0x04, 0x18, 0x04, 0x78},  // m
      {0x7C, 0x08, 0x04, 0x04, 0x78},  // n
      {0x38, 0x44, 0x44, 0x44, 0x38},  // o
      {0x7C, 0x14, 0x14, 0x14, 0x08},  // p
      {0x08, 0x14, 0x14, 0x18, 0x7C},  // q
      {0x7C, 0x08, 0x04, 0x04, 0x08},  // r
      {0x48, 0x54, 0x54, 0x54, 0x20},  // s
      {0x04, 0x3F, 0x44, 0x40, 0x20},  // t
      {0x3C, 0x40, 0x40, 0x20, 0x7C},  // u
      {0x1C, 0x20, 0x40, 0x20, 0x1C},  // v
      {0x3C, 0x40, 0x30, 0x40, 0x3C},  // w
      {0x44, 0x28, 0x10, 0x28, 0x44},  // x
      {0x0C, 0x50, 0x50, 0x50, 0x3C},  // y
      {0x44, 0x64, 0x54, 0x4C, 0x44},  // z
      {0x00, 0x08, 0x36, 0x41, 0x00},  // {
      {0x00, 0x00, 0x7F, 0x00, 0x00},  // |
      {0x00, 0x41, 0x36, 0x08, 0x00},  // }
      {0x08, 0x04, 0x08, 0x10, 0x08},  // ~
      {0x00, 0x00, 0x00, 0x00, 0x00},  // DEL
  };
  if (c < 32 || c > 127) {
    c = 127;
  }
  return font[c - 32];
}
