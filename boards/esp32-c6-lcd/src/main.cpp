/**
 * ArdubotOS ESP32-C6-LCD-1.47 board entry (PlatformIO + Arduino).
 *
 * Same launcher + builtin apps as boards/nodemcu/src/main.cpp (counter /
 * info / stopwatch / pong), rendered on this board's onboard ST7789 color
 * panel via the GfxMono adapter (gfx_mono.h) instead of an I2C SSD1306.
 * Two push-buttons (not onboard — wire them yourself):
 *   UP     — navigate (wrap) / app actions   (GPIO18 by default)
 *   SELECT — launch / confirm / (long-press = back)  (GPIO19 by default)
 *
 * Pins come from build/generated/device_config.h (device_config_esp32c6.yaml).
 * This board has no Wi-Fi credentials path wired in (ARDUBOT_TARGET_ESP8266
 * only) — Wi-Fi status always shows off.
 */

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "device_config.h"
#include "gfx_mono.h"
#include "status.h"
#include "pong.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef ARDUBOT_LCD_WIDTH
#define ARDUBOT_LCD_WIDTH 128
#endif
#ifndef ARDUBOT_LCD_HEIGHT
#define ARDUBOT_LCD_HEIGHT 64
#endif
#ifndef ARDUBOT_BTN_UP_GPIO
#define ARDUBOT_BTN_UP_GPIO 18
#endif
#ifndef ARDUBOT_BTN_SELECT_GPIO
#define ARDUBOT_BTN_SELECT_GPIO 19
#endif
#ifndef ARDUBOT_BTN_UP_ACTIVE_LOW
#define ARDUBOT_BTN_UP_ACTIVE_LOW 1
#endif
#ifndef ARDUBOT_BTN_SELECT_ACTIVE_LOW
#define ARDUBOT_BTN_SELECT_ACTIVE_LOW 1
#endif

#define LONG_PRESS_MS 700

/* Shared layout for the app screens (Counter/Info/Stopwatch/status bar): a
 * margin so content isn't flush against the panel edge (the launcher has
 * its own deliberate edge-to-edge design and doesn't use these), and a
 * line pitch sized for 2x text. */
#define APP_PAD 16
#define APP_LINE_H 20

enum AppId : uint8_t {
  APP_LAUNCHER = 0,
  APP_COUNTER,
  APP_INFO,
  APP_STOPWATCH,
  APP_PONG,
  APP_TASKMGR,
  APP_POMODORO,
  APP_COUNT
};

/* Flat single-tone icon silhouettes (fill) with a cutout/accent color for
 * interior details (a lens ring, a clock hand, a d-pad) — the style a
 * reference HMI mockup used: plain gray silhouette on an unselected dark
 * card, flipped to a dark silhouette + white accent on the selected card's
 * solid color fill. Drawn as vector shapes via GfxMono's primitives, not
 * the shared 1-bit app_icon_t bitmap format (that format stays mono
 * because it's also used by the real NodeMCU OLED hardware). Defined
 * below, after `display` exists; forward-declared here so k_apps can
 * reference them by address. */
static void icon_counter(int x, int y, int s, uint16_t fill, uint16_t accent);
static void icon_info(int x, int y, int s, uint16_t fill, uint16_t accent);
static void icon_stopwatch(int x, int y, int s, uint16_t fill, uint16_t accent);
static void icon_pong(int x, int y, int s, uint16_t fill, uint16_t accent);
static void icon_taskmgr(int x, int y, int s, uint16_t fill, uint16_t accent);
static void icon_pomodoro(int x, int y, int s, uint16_t fill, uint16_t accent);

/* Dim an RGB565 color toward black by right-shifting each channel —
 * cheap way to get a muted/dark variant of a color without a second
 * color table. */
static uint16_t dim_color(uint16_t c, int shift) {
  uint16_t r = (uint16_t)((c >> 11) & 0x1F) >> shift;
  uint16_t g = (uint16_t)((c >> 5) & 0x3F) >> shift;
  uint16_t b = (uint16_t)(c & 0x1F) >> shift;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

struct MenuItem {
  AppId id;
  const char* name;
  const char* tag;
  void (*draw_icon)(int x, int y, int size, uint16_t fill, uint16_t accent);
};

static const MenuItem k_apps[] = {
    {APP_COUNTER, "counter", "USR", icon_counter},
    {APP_INFO, "info", "TOL", icon_info},
    {APP_STOPWATCH, "stopwatch", "TOL", icon_stopwatch},
    {APP_PONG, "pong", "GME", icon_pong},
    {APP_TASKMGR, "tasks", "SYS", icon_taskmgr},
    {APP_POMODORO, "pomodoro", "TOL", icon_pomodoro},
};
static const int k_app_count = (int)(sizeof(k_apps) / sizeof(k_apps[0]));

static GfxMono display(ARDUBOT_LCD_WIDTH, ARDUBOT_LCD_HEIGHT);

/* --- icon bodies: flat `fill` silhouette, `accent` for interior details,
 * sized to fill an SxS box --- */

static void icon_counter(int x, int y, int s, uint16_t fill, uint16_t /*accent*/) {
  const int cx = x + s / 2, cy = y + s / 2;
  const int bar_len = (int)(s * 0.7), bar_th = s / 6;
  display.fill_box(cx - bar_len / 2, cy - bar_th / 2, bar_len, bar_th, bar_th / 2, fill);
  display.fill_box(cx - bar_th / 2, cy - bar_len / 2, bar_th, bar_len, bar_th / 2, fill);
}

static void icon_info(int x, int y, int s, uint16_t fill, uint16_t accent) {
  const int r = s / 2;
  display.fill_circle(x + r, y + r, r, fill);
  display.fill_circle(x + r, y + r - r / 2, s / 10 + 1, accent);
  display.fill_box(x + r - s / 10, y + r - r / 8, s / 5, r / 2 + r / 8, s / 10, accent);
}

static void icon_stopwatch(int x, int y, int s, uint16_t fill, uint16_t accent) {
  const int crown_h = s / 7;
  const int r = (s - crown_h) / 2;
  const int cx = x + s / 2, cy = y + crown_h + r;
  display.fill_box(cx - s / 6, y, s / 3, crown_h, crown_h / 2, fill);
  display.fill_circle(cx, cy, r, fill);
  display.fill_box(cx - 1, cy - r + r / 4, 2, r - r / 4 - 2, 0, accent);
  display.fill_circle(cx, cy, 2, accent);
}

static void icon_pong(int x, int y, int s, uint16_t fill, uint16_t accent) {
  display.fill_box(x, y + s / 4, s, s / 2, s / 6, fill);
  display.fill_circle(x + s / 4, y + s / 2, s / 10, accent);
  display.fill_circle(x + (3 * s) / 4, y + s / 2, s / 10, accent);
  display.fill_box(x + s / 2 - s / 10, y + s / 2 - s / 10, s / 5, s / 5, s / 20, accent);
}

/* Mini bar-chart glyph (3 bars of varying height, htop-sparkline-style). */
static void icon_taskmgr(int x, int y, int s, uint16_t fill, uint16_t /*accent*/) {
  const int bar_w = s / 5;
  const int gap = s / 8;
  const int heights[3] = {s / 2, s - s / 10, (int)(s * 0.7)};
  int bx = x + s / 6;
  int i;
  for (i = 0; i < 3; i++) {
    display.fill_box(bx, y + s - heights[i], bar_w, heights[i], 1, fill);
    bx += bar_w + gap;
  }
}

/* Tomato glyph: round body (fill) + a small leaf-stem (accent) on top. */
static void icon_pomodoro(int x, int y, int s, uint16_t fill, uint16_t accent) {
  const int r = (int)(s * 0.42);
  const int cx = x + s / 2, cy = y + s / 2 + s / 10;
  display.fill_circle(cx, cy, r, fill);
  display.fill_box(cx - s / 10, y + s / 12, s / 5, s / 6, 0, accent);
  display.fill_box(cx - s / 4, y + s / 6, s / 2, s / 10, s / 20, accent);
}

static AppId g_app = APP_COUNTER; /* default boot screen for this board */
static int g_sel = 0;
static int32_t g_count = 0;
static uint32_t g_boot_ms = 0;

static bool g_sw_running = false;
static uint8_t g_sw_h = 0, g_sw_m = 0, g_sw_s = 0;
static uint32_t g_sw_last_ms = 0;

/* Pomodoro: counts down from POMODORO_TOTAL_S to 0, then flashes "DONE"
 * until UP/SELECT is pressed. Fixed at 1 minute for now, per the initial
 * ask — POMODORO_TOTAL_S is the one place to change that later (a real
 * 25-minute work interval, configurable breaks, etc.). */
#define POMODORO_TOTAL_S 60
static int32_t g_pomo_remaining_s = POMODORO_TOTAL_S;
static bool g_pomo_running = false;
static bool g_pomo_done = false;
static uint32_t g_pomo_last_tick_ms = 0;
static bool g_pomo_flash_on = false;
static uint32_t g_pomo_flash_last_ms = 0;

static pong_t g_pong;

static bool g_up_down = false;
static bool g_sel_down = false;
static uint32_t g_sel_down_ms = 0;
static bool g_sel_long_fired = false;
static bool g_dirty = true;

static bool pin_pressed(int gpio, int active_low) {
  const int level = digitalRead(gpio);
  return active_low ? (level == LOW) : (level == HIGH);
}

static void mark_dirty(void) { g_dirty = true; }

/* These screens draw their static labels once and only clear+redraw a
 * small dynamic rect on each periodic tick (see draw_info()/
 * draw_stopwatch()) to avoid full-screen flicker. Reset to false whenever
 * the screen is (re-)entered so the static content, which another screen's
 * display.clear() just wiped, actually gets redrawn. */
static bool g_counter_static_drawn = false;
static bool g_info_static_drawn = false;
static bool g_stopwatch_static_drawn = false;
static bool g_pomo_static_drawn = false;
static int g_pomo_bar_prev_fill = -1;
static char g_pomo_status_prev[12] = "";

/* Previously-drawn dynamic line text, per screen — draw_text_diff() below
 * compares against these so an unchanged character is never touched.
 * Empty string forces every character to be (re)drawn, used right after
 * a screen is (re-)entered. */
static char g_info_line1_prev[22] = "";
static char g_stopwatch_line1_prev[22] = "";
static char g_stopwatch_line2_prev[22] = "";

/* --- Task Manager state --- */
#define TASKMGR_MAX_TASKS 16
#define TASKMGR_MAX_VISIBLE 10
#define TASKMGR_HEAP_BAR_W 96
#define TASKMGR_HEAP_BAR_H 14
static TaskStatus_t g_tm_cur[TASKMGR_MAX_TASKS];
static TaskStatus_t g_tm_prev[TASKMGR_MAX_TASKS];
static UBaseType_t g_tm_cur_n = 0;
static UBaseType_t g_tm_prev_n = 0;
static uint32_t g_tm_cur_total = 0;
static uint32_t g_tm_prev_total = 0;
static bool g_tm_have_prev = false;
static int g_tm_scroll = 0;
static bool g_taskmgr_static_drawn = false;
static char g_tm_sys1_prev[22] = "";
static char g_tm_sys2_prev[22] = "";
static char g_tm_row_prev[TASKMGR_MAX_VISIBLE][28];
static int g_tm_heap_bar_prev_fill = -1; /* -1 forces a redraw on first draw */

static void taskmgr_reset(void) {
  int i;
  g_taskmgr_static_drawn = false;
  g_tm_have_prev = false;
  g_tm_scroll = 0;
  g_tm_heap_bar_prev_fill = -1;
  g_tm_sys1_prev[0] = g_tm_sys2_prev[0] = '\0';
  for (i = 0; i < TASKMGR_MAX_VISIBLE; i++) {
    g_tm_row_prev[i][0] = '\0';
  }
}

/* Outlined used/total bar — fill color shifts green -> orange -> red as
 * `used` approaches `total`. Only actually redraws the fill when its pixel
 * width changes, so a heap size that isn't currently moving doesn't
 * repaint every tick. The outline itself is drawn once by the caller. */
static void draw_progress_bar(int x, int y, int w, int h, uint32_t used, uint32_t total,
                              int* prev_fill) {
  const int inner_w = w - 2;
  const int fill_w = (total > 0) ? (int)(((uint64_t)used * inner_w) / total) : 0;
  const uint32_t pct = (total > 0) ? (used * 100u / total) : 0;
  uint16_t color = RGB565_GREEN;

  if (fill_w == *prev_fill) {
    return;
  }
  *prev_fill = fill_w;

  if (pct >= 90) {
    color = RGB565_RED;
  } else if (pct >= 70) {
    color = RGB565_ORANGE;
  }

  display.fill_box(x + 1, y + 1, inner_w, h - 2, 0, RGB565_NAVY);
  if (fill_w > 0) {
    display.fill_box(x + 1, y + 1, fill_w, h - 2, 0, color);
  }
}

/* Resets the countdown itself back to POMODORO_TOTAL_S, stopped, not
 * flashing — the explicit SELECT "reset" action, distinct from just
 * re-entering the screen (which shouldn't clobber an in-progress timer). */
static void pomodoro_restart(void) {
  g_pomo_remaining_s = POMODORO_TOTAL_S;
  g_pomo_running = false;
  g_pomo_done = false;
  g_pomo_flash_on = false;
  g_pomo_bar_prev_fill = -1;
  g_pomo_status_prev[0] = '\0';
}

/* Forces the next draw_pomodoro() to redraw its static labels and bar from
 * scratch — needed whenever something else's display.clear() just wiped
 * the screen (re-entering the app, or going home), as opposed to
 * pomodoro_restart() above which resets the actual countdown value. */
static void pomodoro_clear_cache(void) {
  g_pomo_static_drawn = false;
  g_pomo_bar_prev_fill = -1;
  g_pomo_status_prev[0] = '\0';
}

static void go_home(void) {
  g_app = APP_LAUNCHER;
  g_counter_static_drawn = false;
  g_info_static_drawn = false;
  g_stopwatch_static_drawn = false;
  pomodoro_clear_cache();
  g_info_line1_prev[0] = '\0';
  g_stopwatch_line1_prev[0] = g_stopwatch_line2_prev[0] = '\0';
  taskmgr_reset();
  mark_dirty();
}

static void open_app(AppId id) {
  g_app = id;
  if (id == APP_PONG) {
    pong_reset(&g_pong);
  }
  if (id == APP_COUNTER) {
    g_counter_static_drawn = false;
  }
  if (id == APP_INFO) {
    g_info_static_drawn = false;
    g_info_line1_prev[0] = '\0';
  }
  if (id == APP_STOPWATCH) {
    g_stopwatch_static_drawn = false;
    g_stopwatch_line1_prev[0] = g_stopwatch_line2_prev[0] = '\0';
  }
  if (id == APP_TASKMGR) {
    taskmgr_reset();
  }
  if (id == APP_POMODORO) {
    pomodoro_clear_cache();
  }
  mark_dirty();
}

/* Redraws only the character cells that actually changed between
 * old_s and new_s (space-padded to whichever is longer), instead of
 * clearing+redrawing the whole line every tick — the clear-then-redraw
 * approach blanked even UNCHANGED digits/letters for a moment each time,
 * which is what read as "flicker" despite each redraw only taking a few
 * milliseconds. old_s is updated in place to new_s's contents. */
static void draw_text_diff(int x, int y, char* old_s, size_t old_cap, const char* new_s,
                           int scale, uint16_t fg, uint16_t bg) {
  const int pitch = 6 * scale;
  size_t i;
  display.set_fg_color(fg);
  for (i = 0; old_s[i] || new_s[i]; i++) {
    char oc = old_s[i] ? old_s[i] : ' ';
    char nc = new_s[i] ? new_s[i] : ' ';
    if (oc != nc) {
      char buf[2] = {nc, '\0'};
      display.fill_box(x + (int)i * pitch, y, pitch, 7 * scale, 0, bg);
      display.draw_text(x + (int)i * pitch, y, buf, scale);
    }
  }
  strncpy(old_s, new_s, old_cap - 1);
  old_s[old_cap - 1] = '\0';
}

static void menu_move(int delta) {
  if (k_app_count <= 0) {
    return;
  }
  g_sel += delta;
  while (g_sel < 0) {
    g_sel += k_app_count;
  }
  while (g_sel >= k_app_count) {
    g_sel -= k_app_count;
  }
  mark_dirty();
}

static void board_set_pixel(int px, int py, bool on, void* user) {
  GfxMono* d = (GfxMono*)user;
  if (d && on) {
    d->set_pixel(px, py, true);
  }
}

/* app_status_blit() always draws its battery/wifi icons flush at x=right
 * edge, y=0 — no way to inset them. This wrapper adds APP_PAD of margin by
 * offsetting every pixel it's given; paired with passing
 * (ARDUBOT_LCD_WIDTH - APP_PAD) as its display_w below so the icons shift
 * left too. */
static void board_set_pixel_padded(int px, int py, bool on, void* user) {
  GfxMono* d = (GfxMono*)user;
  if (d && on) {
    d->set_pixel(px, py + APP_PAD, true);
  }
}

static app_status_link_t g_link = APP_STATUS_LINK_OFF;
static int g_rssi = -127;

static void poll_wifi(void) {}

static void draw_status(void) {
  app_status_blit(ARDUBOT_LCD_WIDTH - APP_PAD, app_status_battery_percent(), g_link, g_rssi,
                  board_set_pixel_padded, &display);
}

/* Flat-style launcher (320x172 physical, scale=1, no padding — see
 * device_config_esp32c6.yaml), matching a flat white/cyan/orange HMI
 * reference:
 *   y=0..24     status bar: white battery (left), white clock (center)
 *   y=24        separator
 *   y=25..108   carousel: 3 cards — unselected = dark + thin outline +
 *               gray icon silhouette; selected = solid cyan fill + dark
 *               icon silhouette with white accent details
 *   y=109..119  pagination dots, centered under the SELECTED card
 *   y=120..137  app title (white, 2x text)
 *   y=138..151  subtitle (gray, selected app's tag)
 *   y=152       separator
 *   y=153..172  action bar: [A] Next (orange) / [B] Open (cyan) badges
 * Always exactly 3 visible cards regardless of k_app_count — index wraps. */
static void draw_launcher(void) {
  const int w = ARDUBOT_LCD_WIDTH; /* 320 */
  int center_tile_x = 0, center_tile_bottom = 0;

  display.clear();

  /* === status bar: y 0..24 (white battery left, white clock center) === */
  display.draw_box(6, 5, 26, 14, 2, RGB565_WHITE);
  display.fill_box(32, 9, 3, 6, 0, RGB565_WHITE);
  {
    const int level = (app_status_battery_percent() * 4) / 100;
    int i;
    for (i = 0; i < 4; i++) {
      display.fill_box(9 + i * 5, 8, 3, 8, 0, i < level ? RGB565_WHITE : dim_color(RGB565_WHITE, 3));
    }
  }
  {
    /* No RTC on this board profile — show uptime as a clock-style MM:SS. */
    char clock_buf[8];
    const uint32_t up_s = (millis() - g_boot_ms) / 1000u;
    snprintf(clock_buf, sizeof(clock_buf), "%02u:%02u", (unsigned)((up_s / 60u) % 100u),
             (unsigned)(up_s % 60u));
    display.set_fg_color(RGB565_WHITE);
    display.draw_text((w - display.text_width(clock_buf, 2)) / 2, 5, clock_buf, 2);
  }
  display.draw_hline(0, 24, w, dim_color(RGB565_WHITE, 4));

  /* === carousel: y 25..108, 3 cards, selected one bigger === */
  {
    /* Budget: 320 wide, 24px reserved each side for chevrons, leaving 272
     * for 2 side tiles + center tile + 2 gaps: 54+12+140+12+54 = 272. */
    const int side_margin = 24;
    const int tile_gap = 12;
    const int side_w = 54;
    const int side_h = 70;
    const int center_w = 140;
    const int center_h = 85;
    const int row_mid = 25 + (108 - 25) / 2;
    const int tile_w[3] = {side_w, center_w, side_w};
    const int tile_h[3] = {side_h, center_h, side_h};
    int tile_x[3];
    int tile_y[3];
    int slot;

    tile_x[1] = (w - center_w) / 2;
    tile_x[0] = tile_x[1] - tile_gap - side_w;
    tile_x[2] = tile_x[1] + center_w + tile_gap;
    tile_y[0] = row_mid - side_h / 2;
    tile_y[1] = row_mid - center_h / 2;
    tile_y[2] = row_mid - side_h / 2;
    center_tile_x = tile_x[1] + center_w / 2;
    center_tile_bottom = tile_y[1] + center_h;

    for (slot = 0; slot < 3; slot++) {
      int idx = ((g_sel + (slot - 1)) % k_app_count + k_app_count) % k_app_count;
      const bool sel = (slot == 1);
      const MenuItem* app = &k_apps[idx];
      const int icon_render_size = sel ? 64 : 48;
      const int icon_x = tile_x[slot] + (tile_w[slot] - icon_render_size) / 2;
      const int icon_y = tile_y[slot] + (tile_h[slot] - icon_render_size) / 2;
      const uint16_t icon_fill = sel ? RGB565_BLACK : dim_color(RGB565_WHITE, 2);
      const uint16_t icon_accent = sel ? RGB565_WHITE : RGB565_BLACK;

      if (sel) {
        display.fill_box(tile_x[slot], tile_y[slot], tile_w[slot], tile_h[slot], 10, RGB565_CYAN);
      } else {
        display.fill_box(tile_x[slot], tile_y[slot], tile_w[slot], tile_h[slot], 10, RGB565_BLACK);
        display.draw_box(tile_x[slot], tile_y[slot], tile_w[slot], tile_h[slot], 10,
                         dim_color(RGB565_WHITE, 3));
      }
      app->draw_icon(icon_x, icon_y, icon_render_size, icon_fill, icon_accent);
    }

    if (side_margin > 0) {
      display.set_fg_color(dim_color(RGB565_WHITE, 2));
      display.draw_text(side_margin / 2 - 3, row_mid - 3, "<", 2);
      display.draw_text(w - side_margin / 2 - 9, row_mid - 3, ">", 2);
    }
  }

  /* === pagination dots: centered under the selected card === */
  {
    const int dot_pitch = 10;
    const int dots_w = (k_app_count - 1) * dot_pitch;
    int dx = center_tile_x - dots_w / 2;
    int i;
    for (i = 0; i < k_app_count; i++) {
      display.fill_circle(dx, center_tile_bottom + 7, i == g_sel ? 3 : 2,
                          i == g_sel ? RGB565_CYAN : dim_color(RGB565_WHITE, 3));
      dx += dot_pitch;
    }
  }

  /* === title: selected app's name (white, 2x) === */
  {
    const char* name = k_apps[g_sel].name;
    display.set_fg_color(RGB565_WHITE);
    display.draw_text((w - display.text_width(name, 2)) / 2, 120, name, 2);
  }

  /* === subtitle: selected app's tag, gray === */
  {
    const char* tag = k_apps[g_sel].tag;
    display.set_fg_color(dim_color(RGB565_WHITE, 2));
    display.draw_text((w - display.text_width(tag)) / 2, 142, tag);
  }

  display.draw_hline(0, 152, w, dim_color(RGB565_WHITE, 4));

  /* === action bar: [A] Next (orange) / [B] Open (cyan) badges === */
  display.set_fg_color(RGB565_ORANGE);
  display.draw_box(16, 155, 16, 16, 3, RGB565_ORANGE);
  display.draw_text(21, 159, "A");
  display.draw_text(38, 158, "Next", 2);
  display.set_fg_color(RGB565_CYAN);
  {
    const char* label = "Open";
    const int label_w = display.text_width(label, 2);
    const int box_x = w - 16 - 16 - 6 - label_w;
    display.draw_box(box_x, 155, 16, 16, 3, RGB565_CYAN);
    display.draw_text(box_x + 5, 159, "B");
    display.draw_text(box_x + 16 + 6, 158, label, 2);
  }

  display.display();
}

/* Dynamic UI: the number's text scale isn't fixed — it's recomputed from
 * how many digits currently need to fit in the available width, so it
 * stays as large as possible (1-2 digits get the biggest scale; it shrinks
 * automatically as the count grows more digits instead of running off the
 * edge or needing a hardcoded cutoff) and auto-resizes if this ever runs
 * on a different ARDUBOT_LCD_WIDTH. Color and the gauge bar both react to
 * the sign of the value for at-a-glance feedback, not just the number. */
static void draw_counter(void) {
  char buf[16];
  const int len = snprintf(buf, sizeof(buf), "%ld", (long)g_count);
  const int max_w = ARDUBOT_LCD_WIDTH - 2 * APP_PAD;
  int scale = max_w / (len * 6);
  const int max_scale = 6;
  if (scale > max_scale) {
    scale = max_scale;
  }
  if (scale < 1) {
    scale = 1;
  }
  const int num_y = APP_PAD + APP_LINE_H + 10;
  const int num_area_h = 7 * max_scale; /* tallest the number can ever get */
  const int bar_y = num_y + num_area_h + 10;
  const int bar_h = 16;
  const int bar_x = APP_PAD;
  const int bar_w = max_w;
  const int cx = bar_x + bar_w / 2;

  if (!g_counter_static_drawn) {
    display.clear();
    display.set_fg_color(RGB565_CYAN);
    display.draw_text(APP_PAD, APP_PAD, "Counter", 2);
    display.draw_box(bar_x, bar_y, bar_w, bar_h, 2, RGB565_DARKGREY);
    display.set_fg_color(RGB565_DARKGREY);
    display.draw_text(APP_PAD, ARDUBOT_LCD_HEIGHT - APP_PAD - 2 * APP_LINE_H, "Up:+  Sel:-", 2);
    display.draw_text(APP_PAD, ARDUBOT_LCD_HEIGHT - APP_PAD - APP_LINE_H, "hold Sel: back", 2);
    draw_status();
    g_counter_static_drawn = true;
  }

  /* Clear the number's full possible footprint (not just this scale's
   * actual size) so a shrink from a previous, wider/taller render doesn't
   * leave stray digits behind. */
  display.fill_box(APP_PAD, num_y, max_w, num_area_h, 0, RGB565_NAVY);
  {
    const uint16_t num_color =
        g_count > 0 ? RGB565_GREEN : (g_count < 0 ? RGB565_RED : RGB565_WHITE);
    const int num_x = (ARDUBOT_LCD_WIDTH - len * 6 * scale) / 2;
    display.set_fg_color(num_color);
    display.draw_text(num_x, num_y, buf, scale);
  }

  /* Gauge: fills from the center tick toward either side, clamped to a
   * +/-20 visual range (the number itself has no such limit — this is
   * just "how far from zero, roughly" at a glance). */
  {
    const int half_w = bar_w / 2 - 1;
    int32_t clamped = g_count;
    if (clamped > 20) {
      clamped = 20;
    }
    if (clamped < -20) {
      clamped = -20;
    }
    const int fill = (int)((int32_t)(clamped < 0 ? -clamped : clamped) * half_w / 20);
    const uint16_t fill_color = g_count >= 0 ? RGB565_GREEN : RGB565_RED;
    display.fill_box(bar_x + 1, bar_y + 1, bar_w - 2, bar_h - 2, 0, RGB565_NAVY);
    if (fill > 0) {
      if (g_count >= 0) {
        display.fill_box(cx, bar_y + 1, fill, bar_h - 2, 0, fill_color);
      } else {
        display.fill_box(cx - fill, bar_y + 1, fill, bar_h - 2, 0, fill_color);
      }
    }
    display.fill_box(cx - 1, bar_y, 2, bar_h, 0, RGB565_WHITE);
  }

  draw_status();
  display.display();
}

/* Info redraws its uptime/button-state lines every second (see
 * last_info_tick in loop()) to stay live without needing a button press.
 * Doing that via a full display.clear() + redraw-everything, like the
 * other screens, repaints the whole panel every tick - visible as the
 * screen flashing/blinking once a second. Instead: draw the static labels
 * (title, device line, disp size, wiring-check heading) ONCE when the
 * screen is entered (g_info_static_drawn, declared near go_home()/
 * open_app() which reset it), and on every tick only clear+redraw the two
 * small rects that actually change. */
static void draw_info(void) {
  char line[22];
  const uint32_t up_s = (millis() - g_boot_ms) / 1000u;
  const int dyn1_y = APP_PAD + 3 * APP_LINE_H;

  if (!g_info_static_drawn) {
    display.clear();
    display.set_fg_color(RGB565_CYAN);
    display.draw_text(APP_PAD, APP_PAD, "Device Info", 2);
    display.set_fg_color(RGB565_WHITE);
    display.draw_text(APP_PAD, APP_PAD + APP_LINE_H, "ArdubotOS esp32-c6", 2);
    snprintf(line, sizeof(line), "Disp %ux%u", (unsigned)ARDUBOT_LCD_WIDTH,
             (unsigned)ARDUBOT_LCD_HEIGHT);
    display.draw_text(APP_PAD, APP_PAD + 2 * APP_LINE_H, line, 2);
    draw_status();
    g_info_static_drawn = true;
  }

  snprintf(line, sizeof(line), "Up:%lus Apps:%d", (unsigned long)up_s, k_app_count);
  draw_text_diff(APP_PAD, dyn1_y, g_info_line1_prev, sizeof(g_info_line1_prev), line, 2,
                RGB565_WHITE, RGB565_NAVY);
  display.set_fg_color(RGB565_WHITE);

  display.display();
}

/* Same flicker fix as draw_info(): static labels drawn once, the
 * time/state readout cleared+redrawn as a small rect each tick instead of
 * the whole screen. g_stopwatch_static_drawn is declared near go_home()/
 * open_app(), which reset it. */
static void draw_stopwatch(void) {
  char line[22];
  const int dyn_y = APP_PAD + APP_LINE_H;

  if (!g_stopwatch_static_drawn) {
    display.clear();
    display.set_fg_color(RGB565_CYAN);
    display.draw_text(APP_PAD, APP_PAD, "Stopwatch", 2);
    display.set_fg_color(RGB565_DARKGREY);
    display.draw_text(APP_PAD, APP_PAD + 3 * APP_LINE_H, "Up:tog Sel:rst", 2);
    display.set_fg_color(RGB565_WHITE);
    draw_status();
    g_stopwatch_static_drawn = true;
  }

  snprintf(line, sizeof(line), "%02u:%02u:%02u", (unsigned)g_sw_h, (unsigned)g_sw_m,
           (unsigned)g_sw_s);
  draw_text_diff(APP_PAD, dyn_y, g_stopwatch_line1_prev, sizeof(g_stopwatch_line1_prev), line, 2,
                RGB565_WHITE, RGB565_NAVY);
  draw_text_diff(APP_PAD, dyn_y + APP_LINE_H, g_stopwatch_line2_prev,
                sizeof(g_stopwatch_line2_prev), g_sw_running ? "RUN" : "STP", 2,
                g_sw_running ? RGB565_GREEN : RGB565_ORANGE, RGB565_NAVY);
  display.set_fg_color(RGB565_WHITE);

  display.display();
}

/* Counts down from POMODORO_TOTAL_S to 0 (ticked once/second in loop()).
 * Big auto-scaled number (same technique as draw_counter()'s dynamic
 * sizing), a depleting progress bar, and a RUNNING/PAUSED status line.
 * At 0 it switches to a full-screen flash ("DONE!" alternating navy/red)
 * until UP or SELECT is pressed — on_up()/on_select() call
 * pomodoro_restart(), which is what actually clears g_pomo_done. */
static void draw_pomodoro(void) {
  const int max_w = ARDUBOT_LCD_WIDTH - 2 * APP_PAD;
  const int num_y = APP_PAD + APP_LINE_H + 10;
  const int num_area_h = 7 * 6; /* tallest the number can ever get (scale 6) */
  const int bar_y = num_y + num_area_h + 10;
  const int bar_h = 16;
  const int bar_x = APP_PAD;
  const int status_y = bar_y + bar_h + 10;

  if (g_pomo_done) {
    /* Full-screen flash, bypassing the normal static/dynamic split above —
     * every pixel changes every flash, so there's nothing to save by
     * diffing here. */
    display.set_bg_color(g_pomo_flash_on ? RGB565_RED : RGB565_NAVY);
    display.clear();
    display.set_fg_color(RGB565_WHITE);
    {
      const char* msg = "DONE!";
      display.draw_text((ARDUBOT_LCD_WIDTH - display.text_width(msg, 4)) / 2,
                        (ARDUBOT_LCD_HEIGHT / 2) - 30, msg, 4);
    }
    display.set_fg_color(RGB565_DARKGREY);
    {
      const char* hint = "press a button";
      display.draw_text((ARDUBOT_LCD_WIDTH - display.text_width(hint, 1)) / 2,
                        (ARDUBOT_LCD_HEIGHT / 2) + 30, hint, 1);
    }
    display.set_bg_color(RGB565_NAVY); /* restore the shared default */
    display.display();
    return;
  }

  if (!g_pomo_static_drawn) {
    display.clear();
    display.set_fg_color(RGB565_CYAN);
    display.draw_text(APP_PAD, APP_PAD, "Pomodoro", 2);
    display.draw_box(bar_x, bar_y, max_w, bar_h, 2, RGB565_DARKGREY);
    display.set_fg_color(RGB565_DARKGREY);
    display.draw_text(APP_PAD, ARDUBOT_LCD_HEIGHT - APP_PAD - APP_LINE_H,
                      "Up:start/pause Sel:reset", 1);
    draw_status();
    g_pomo_static_drawn = true;
  }

  {
    char buf[4];
    const int len = snprintf(buf, sizeof(buf), "%ld", (long)g_pomo_remaining_s);
    int scale = max_w / (len * 6);
    if (scale > 6) {
      scale = 6;
    }
    if (scale < 1) {
      scale = 1;
    }
    const uint16_t color = g_pomo_remaining_s <= 10
                               ? RGB565_RED
                               : (g_pomo_remaining_s <= 20 ? RGB565_ORANGE : RGB565_WHITE);
    display.fill_box(APP_PAD, num_y, max_w, num_area_h, 0, RGB565_NAVY);
    display.set_fg_color(color);
    display.draw_text((ARDUBOT_LCD_WIDTH - len * 6 * scale) / 2, num_y, buf, scale);
  }

  draw_progress_bar(bar_x, bar_y, max_w, bar_h,
                    (uint32_t)(POMODORO_TOTAL_S - g_pomo_remaining_s), (uint32_t)POMODORO_TOTAL_S,
                    &g_pomo_bar_prev_fill);

  draw_text_diff(APP_PAD, status_y, g_pomo_status_prev, sizeof(g_pomo_status_prev),
                g_pomo_running ? "RUNNING" : "PAUSED", 2,
                g_pomo_running ? RGB565_GREEN : RGB565_ORANGE, RGB565_NAVY);
  display.set_fg_color(RGB565_WHITE);

  display.display();
}

static void draw_pong(void) {
  char line[16];
  int y;
  int i;
  display.clear();
  display.set_fg_color(RGB565_DARKGREY);
  for (y = 0; y < PONG_H && y < ARDUBOT_LCD_HEIGHT; y++) {
    display.set_pixel(PONG_W - 2, y, true);
    display.set_pixel(PONG_W - 1, y, true);
  }
  display.set_fg_color(RGB565_CYAN);
  for (i = 0; i < PONG_PADDLE_H; i++) {
    display.set_pixel(PONG_PADDLE_X, g_pong.paddle_y + i, true);
    display.set_pixel(PONG_PADDLE_X + 1, g_pong.paddle_y + i, true);
  }
  display.set_fg_color(RGB565_YELLOW);
  for (y = 0; y < PONG_BALL; y++) {
    for (i = 0; i < PONG_BALL; i++) {
      display.set_pixel(g_pong.ball_x + i, g_pong.ball_y + y, true);
    }
  }
  display.set_fg_color(RGB565_WHITE);
  if (g_pong.game_over) {
    display.draw_text(46, 4, "END");
    snprintf(line, sizeof(line), "S:%u Sel", (unsigned)g_pong.score);
    display.draw_text(28, 16, line);
  } else {
    snprintf(line, sizeof(line), "%u", (unsigned)g_pong.score);
    display.draw_text(0, 0, line);
  }
  draw_status();
  display.display();
}

static char taskmgr_state_char(eTaskState s) {
  switch (s) {
    case eRunning:
      return 'R';
    case eReady:
      return 'Y';
    case eBlocked:
      return 'B';
    case eSuspended:
      return 'S';
    case eDeleted:
      return 'D';
    default:
      return '?';
  }
}

/* -1 if no baseline yet (first sample since entering this screen) or the
 * task is new since the last sample. */
static int taskmgr_cpu_percent(const TaskStatus_t* cur) {
  uint32_t total_delta;
  UBaseType_t i;
  if (!g_tm_have_prev) {
    return -1;
  }
  total_delta = g_tm_cur_total - g_tm_prev_total;
  if (total_delta == 0) {
    return -1;
  }
  for (i = 0; i < g_tm_prev_n; i++) {
    if (g_tm_prev[i].xHandle == cur->xHandle) {
      uint32_t rt_delta = cur->ulRunTimeCounter - g_tm_prev[i].ulRunTimeCounter;
      return (int)(((uint64_t)rt_delta * 100u) / total_delta);
    }
  }
  return -1;
}

/* Real FreeRTOS task list (uxTaskGetSystemState — needs
 * CONFIG_FREERTOS_USE_TRACE_FACILITY, and CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
 * for the CPU%; both are on by default in this framework's sdkconfig).
 * Auto-sizes to ARDUBOT_LCD_WIDTH/HEIGHT instead of a fixed 320x172 layout:
 * row count comes from how much vertical space is actually available, and
 * every row uses the small 1x font so it still fits on a much narrower
 * panel than this board's. UP scrolls when there are more tasks than fit;
 * long-press SELECT (on_back(), global) returns to the launcher. */
static void draw_taskmgr(void) {
  char line[28];
  /* Title (y=APP_PAD, 2x) ends at APP_PAD+14; Heap/CPU lines each take one
   * APP_LINE_H row after that (2x text, 14px tall, so each leaves a few px
   * of gap before the next). header_y sits right after the CPU line ends
   * (APP_PAD+2*APP_LINE_H+14); list_y0 leaves room for the header's own
   * 1x-text height (7px) plus a gap before the first task row — this used
   * to be computed independently of where the CPU line actually ended and
   * overlapped it. */
  const int header_y = APP_PAD + 3 * APP_LINE_H;
  const int list_y0 = header_y + APP_LINE_H - 2; /* header is now 2x text (14px tall) + a gap */
  const int row_h = APP_LINE_H - 2;
  int visible_rows = (ARDUBOT_LCD_HEIGHT - list_y0 - APP_PAD) / row_h;
  int i;

  if (visible_rows > TASKMGR_MAX_VISIBLE) {
    visible_rows = TASKMGR_MAX_VISIBLE;
  }
  if (visible_rows < 1) {
    visible_rows = 1;
  }

  /* Sample current task state, keep the previous sample around for the
   * CPU% delta. */
  memcpy(g_tm_prev, g_tm_cur, sizeof(g_tm_cur));
  g_tm_prev_n = g_tm_cur_n;
  g_tm_prev_total = g_tm_cur_total;
  g_tm_cur_n = uxTaskGetSystemState(g_tm_cur, TASKMGR_MAX_TASKS, &g_tm_cur_total);
  if (g_tm_prev_n > 0) {
    g_tm_have_prev = true;
  }

  if (g_tm_scroll > (int)g_tm_cur_n - visible_rows) {
    g_tm_scroll = (int)g_tm_cur_n - visible_rows;
  }
  if (g_tm_scroll < 0) {
    g_tm_scroll = 0;
  }

  if (!g_taskmgr_static_drawn) {
    display.clear();
    display.set_fg_color(RGB565_CYAN);
    display.draw_text(APP_PAD, APP_PAD, "Task Manager", 2);
    display.set_fg_color(RGB565_DARKGREY);
    /* Must stay column-aligned with the row format below
     * ("%-8.8s %c%4d%% %4u"): 8-char name, space, 1-char state, then
     * cpu%/stack. Drawn at the SAME 2x scale as the rows — a 1x header
     * over 2x data rows can never align since the two fonts have
     * different character widths regardless of x position, which is why
     * the columns looked wrong before. */
    display.draw_text(APP_PAD, header_y, "NAME     SCPU%  STK", 2);
    /* Heap used/total bar, right-aligned on the Heap text's row. */
    display.draw_box(ARDUBOT_LCD_WIDTH - APP_PAD - TASKMGR_HEAP_BAR_W, APP_PAD + APP_LINE_H,
                     TASKMGR_HEAP_BAR_W, TASKMGR_HEAP_BAR_H, 2, RGB565_DARKGREY);
    draw_status();
    g_taskmgr_static_drawn = true;
  }

  {
    const uint32_t heap_total = ESP.getHeapSize();
    const uint32_t heap_used = heap_total - ESP.getFreeHeap();
    snprintf(line, sizeof(line), "Heap:%uK/%uK", (unsigned)(ESP.getFreeHeap() / 1024),
             (unsigned)(heap_total / 1024));
    draw_text_diff(APP_PAD, APP_PAD + APP_LINE_H, g_tm_sys1_prev, sizeof(g_tm_sys1_prev), line, 2,
                  RGB565_WHITE, RGB565_NAVY);
    draw_progress_bar(ARDUBOT_LCD_WIDTH - APP_PAD - TASKMGR_HEAP_BAR_W, APP_PAD + APP_LINE_H,
                      TASKMGR_HEAP_BAR_W, TASKMGR_HEAP_BAR_H, heap_used, heap_total,
                      &g_tm_heap_bar_prev_fill);
  }

  snprintf(line, sizeof(line), "CPU:%uMHz Tasks:%u", (unsigned)getCpuFrequencyMhz(),
           (unsigned)g_tm_cur_n);
  draw_text_diff(APP_PAD, APP_PAD + 2 * APP_LINE_H, g_tm_sys2_prev, sizeof(g_tm_sys2_prev), line,
                2, RGB565_WHITE, RGB565_NAVY);

  for (i = 0; i < visible_rows; i++) {
    const int idx = g_tm_scroll + i;
    const int row_y = list_y0 + i * row_h;
    if (idx >= (int)g_tm_cur_n) {
      draw_text_diff(APP_PAD, row_y, g_tm_row_prev[i], sizeof(g_tm_row_prev[i]), "", 2,
                    RGB565_WHITE, RGB565_NAVY);
      continue;
    }
    const TaskStatus_t* t = &g_tm_cur[idx];
    const int cpu = taskmgr_cpu_percent(t);
    if (cpu >= 0) {
      snprintf(line, sizeof(line), "%-8.8s %c%4d%% %4u", t->pcTaskName,
               taskmgr_state_char(t->eCurrentState), cpu, (unsigned)t->usStackHighWaterMark);
    } else {
      snprintf(line, sizeof(line), "%-8.8s %c  -- %4u", t->pcTaskName,
               taskmgr_state_char(t->eCurrentState), (unsigned)t->usStackHighWaterMark);
    }
    draw_text_diff(APP_PAD, row_y, g_tm_row_prev[i], sizeof(g_tm_row_prev[i]), line, 2,
                  RGB565_WHITE, RGB565_NAVY);
  }

  display.display();
}

static void redraw(void) {
  static uint32_t last_paint_ms = 0;
  static bool have_painted = false;
  if (!g_dirty) {
    return;
  }
  const uint32_t now = millis();
  if (!app_status_redraw_due(now, last_paint_ms, have_painted)) {
    return; /* stay dirty; paint on a later pass */
  }
  last_paint_ms = now;
  have_painted = true;
  g_dirty = false;
  switch (g_app) {
    case APP_LAUNCHER:
      draw_launcher();
      break;
    case APP_COUNTER:
      draw_counter();
      break;
    case APP_INFO:
      draw_info();
      break;
    case APP_STOPWATCH:
      draw_stopwatch();
      break;
    case APP_PONG:
      draw_pong();
      break;
    case APP_TASKMGR:
      draw_taskmgr();
      break;
    case APP_POMODORO:
      draw_pomodoro();
      break;
    default:
      break;
  }
}

static void on_up(void) {
  switch (g_app) {
    case APP_LAUNCHER:
      menu_move(+1); /* side-scroll next cell — same as sim Up/Right */
      break;
    case APP_COUNTER:
      g_count++;
      mark_dirty();
      break;
    case APP_INFO:
      mark_dirty();
      break;
    case APP_STOPWATCH:
      g_sw_running = !g_sw_running;
      if (g_sw_running) {
        g_sw_last_ms = millis();
      }
      mark_dirty();
      break;
    case APP_PONG:
      if (!g_pong.game_over) {
        pong_paddle_up(&g_pong);
        mark_dirty();
      }
      break;
    case APP_TASKMGR:
      g_tm_scroll++; /* clamped back into range in draw_taskmgr() */
      mark_dirty();
      break;
    case APP_POMODORO:
      if (g_pomo_done) {
        pomodoro_restart(); /* "press a button" dismisses the flash */
      } else {
        g_pomo_running = !g_pomo_running;
        if (g_pomo_running) {
          g_pomo_last_tick_ms = millis();
        }
      }
      mark_dirty();
      break;
    default:
      break;
  }
}

static void on_select(void) {
  switch (g_app) {
    case APP_LAUNCHER:
      open_app(k_apps[g_sel].id);
      break;
    case APP_COUNTER:
      g_count--;
      mark_dirty();
      break;
    case APP_INFO:
      mark_dirty();
      break;
    case APP_STOPWATCH:
      g_sw_h = g_sw_m = g_sw_s = 0;
      g_sw_running = false;
      mark_dirty();
      break;
    case APP_PONG:
      if (g_pong.game_over) {
        pong_reset(&g_pong);
      } else {
        pong_paddle_down(&g_pong);
      }
      mark_dirty();
      break;
    case APP_TASKMGR:
      mark_dirty(); /* no-op action; long-press SELECT (on_back) goes home */
      break;
    case APP_POMODORO:
      pomodoro_restart(); /* tap SELECT = reset (hint text: "Sel:reset") */
      mark_dirty();
      break;
    default:
      break;
  }
}

static void on_back(void) {
  if (g_app != APP_LAUNCHER) {
    go_home();
  }
}

static void poll_buttons(void) {
  const bool up = pin_pressed(ARDUBOT_BTN_UP_GPIO, ARDUBOT_BTN_UP_ACTIVE_LOW);
  const bool sel = pin_pressed(ARDUBOT_BTN_SELECT_GPIO, ARDUBOT_BTN_SELECT_ACTIVE_LOW);

  if (up && !g_up_down) {
    on_up();
  }
  g_up_down = up;

  const uint32_t now = millis();

  if (sel && !g_sel_down) {
    g_sel_down = true;
    g_sel_down_ms = now;
    g_sel_long_fired = false;
  } else if (sel && g_sel_down && !g_sel_long_fired) {
    if (now - g_sel_down_ms >= LONG_PRESS_MS) {
      g_sel_long_fired = true;
      on_back();
    }
  } else if (!sel && g_sel_down) {
    if (!g_sel_long_fired) {
      on_select();
    }
    g_sel_down = false;
  }
}

static void stopwatch_tick(void) {
  if (g_app != APP_STOPWATCH || !g_sw_running) {
    return;
  }
  const uint32_t now = millis();
  while (now - g_sw_last_ms >= 1000u) {
    g_sw_last_ms += 1000u;
    g_sw_s++;
    if (g_sw_s >= 60) {
      g_sw_s = 0;
      g_sw_m++;
      if (g_sw_m >= 60) {
        g_sw_m = 0;
        g_sw_h++;
        if (g_sw_h >= 100) {
          g_sw_h = 0;
        }
      }
    }
    mark_dirty();
  }
}

static void pomodoro_tick(void) {
  if (g_app != APP_POMODORO) {
    return;
  }
  const uint32_t now = millis();

  if (g_pomo_done) {
    /* Flash cadence — independent of the countdown, only runs once done. */
    if (now - g_pomo_flash_last_ms >= 400u) {
      g_pomo_flash_last_ms = now;
      g_pomo_flash_on = !g_pomo_flash_on;
      mark_dirty();
    }
    return;
  }

  if (!g_pomo_running) {
    return;
  }
  while (now - g_pomo_last_tick_ms >= 1000u) {
    g_pomo_last_tick_ms += 1000u;
    g_pomo_remaining_s--;
    mark_dirty();
    if (g_pomo_remaining_s <= 0) {
      g_pomo_remaining_s = 0;
      g_pomo_running = false;
      g_pomo_done = true;
      g_pomo_flash_last_ms = now;
      g_pomo_flash_on = true;
      break;
    }
  }
}

static void pong_tick(void) {
  static uint32_t last_ms = 0;
  const uint32_t now = millis();
  if (g_app != APP_PONG || g_pong.game_over) {
    return;
  }
  if (now - last_ms < (uint32_t)APP_STATUS_REDRAW_MS) {
    return;
  }
  last_ms = now;
  if (pin_pressed(ARDUBOT_BTN_UP_GPIO, ARDUBOT_BTN_UP_ACTIVE_LOW)) {
    pong_paddle_up(&g_pong);
  }
  if (pin_pressed(ARDUBOT_BTN_SELECT_GPIO, ARDUBOT_BTN_SELECT_ACTIVE_LOW)) {
    pong_paddle_down(&g_pong);
  }
  pong_step(&g_pong);
  mark_dirty();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("ArdubotOS ESP32-C6-LCD — launcher + builtins"));
  Serial.printf("LCD %dx%d (ST7789, GfxMono adapter)\n", ARDUBOT_LCD_WIDTH, ARDUBOT_LCD_HEIGHT);
  Serial.printf("BTN up=GPIO%d select=GPIO%d (long-select = back)\n", ARDUBOT_BTN_UP_GPIO,
                ARDUBOT_BTN_SELECT_GPIO);
  pinMode(ARDUBOT_BTN_UP_GPIO, INPUT_PULLUP);
  pinMode(ARDUBOT_BTN_SELECT_GPIO, INPUT_PULLUP);

  Serial.println(F("Wi-Fi not wired on this board profile"));

  display.set_bg_color(RGB565_NAVY);
  display.set_fg_color(RGB565_WHITE);
  if (!display.begin()) {
    Serial.println(F("ST7789 init failed — check wiring"));
    for (;;) {
      delay(1000);
    }
  }

  g_boot_ms = millis();
  app_status_set_battery_percent(100); /* USB-powered */
  mark_dirty();
  redraw();
}

void loop() {
  poll_buttons();
  poll_wifi();
  stopwatch_tick();
  pomodoro_tick();
  pong_tick();
  /* Info's uptime line wants its own ~1/s tick, independent of button
   * events, to stay live while the screen is just sitting open. */
  static uint32_t last_info_tick;
  if (g_app == APP_INFO && millis() - last_info_tick > 1000u) {
    last_info_tick = millis();
    mark_dirty();
  }
  /* Task Manager's heap/CPU%/task list also need a live tick. */
  static uint32_t last_taskmgr_tick;
  if (g_app == APP_TASKMGR && millis() - last_taskmgr_tick > 1000u) {
    last_taskmgr_tick = millis();
    mark_dirty();
  }
  redraw();
  delay(15);
}
