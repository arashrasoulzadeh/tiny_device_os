#include "boot_splash.h"

#include "app_helper.h"
#include "fw/ui.h"
#include "theme.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(__has_include)
#if __has_include("build_info.h")
#include "build_info.h"
#endif
#endif

#ifndef ARDUBOT_GIT_HASH
#define ARDUBOT_GIT_HASH "00000000"
#endif

#if !defined(ARDUBOT_TARGET_ESP32)
#include "sim_video.h"
#endif

static app_display_t g_disp;
static bool g_open;

static void hash_chars(const char *hash, char out[9]) {
  size_t n = 0;
  if (!hash) {
    hash = "";
  }
  while (hash[n] != '\0') {
    n++;
  }
  if (n >= 8) {
    memcpy(out, hash + (n - 8), 8);
  } else {
    memset(out, '0', 8 - n);
    if (n > 0) {
      memcpy(out + (8 - n), hash, n);
    }
  }
  out[8] = '\0';
}

int boot_splash_format_title(char *buf, size_t cap, const char *hash) {
  char h[9];
  int n;
  if (!buf || cap == 0) {
    return -1;
  }
  hash_chars(hash, h);
  n = snprintf(buf, cap, "ArdubotOS %s", h);
  if (n < 0 || (size_t)n >= cap) {
    buf[0] = '\0';
    return -1;
  }
  return n;
}

int boot_splash_format_status(char *buf, size_t cap, const char *label) {
  size_t i = 0;
  if (!buf || cap == 0) {
    return -1;
  }
  if (!label) {
    label = "";
  }
  while (label[i] != '\0' && i + 1 < cap) {
    buf[i] = label[i];
    i++;
  }
  if (label[i] != '\0') {
    buf[0] = '\0';
    return -1;
  }
  buf[i] = '\0';
  return (int)i;
}

int boot_splash_bar_fill(int done, int total, int width_px) {
  return app_bar_fill_px((int32_t)done, (int32_t)total, width_px);
}

void boot_splash_open(void) {
  if (g_open) {
    return;
  }
  memset(&g_disp, 0, sizeof(g_disp));
  if (app_display_init(&g_disp, "/dev/display0") != 0) {
    return;
  }
  g_open = true;
}

void boot_splash_show(const char *label, int done, int total) {
  char title[32];
  char status[48];
  int scale;
  int gap;
  int bar_h;
  int line_h;
  int block;
  int y;
  int bar_x;
  int bar_w;
  int fill;
  int tw;

  if (!g_open) {
    boot_splash_open();
  }
  if (!g_open || !g_disp.initialized) {
    return;
  }

  if (boot_splash_format_title(title, sizeof(title), ARDUBOT_GIT_HASH) < 0) {
    title[0] = '\0';
  }
  if (boot_splash_format_status(status, sizeof(status), label) < 0) {
    status[0] = '\0';
  }

  scale = (g_disp.height > 64) ? 2 : 1;
  gap = (g_disp.height > 64) ? 10 : 2;
  bar_h = (g_disp.height > 64) ? 8 : 3;
  line_h = 8 * scale;
  block = line_h + gap + bar_h + gap + line_h;
  y = app_center_in(0, g_disp.height, block);
  if (y < 0) {
    y = 0;
  }

  bar_x = (g_disp.width > 64) ? 16 : 4;
  bar_w = g_disp.width - bar_x * 2;
  if (bar_w < 1) {
    bar_w = g_disp.width;
    bar_x = 0;
  }
  fill = boot_splash_bar_fill(done, total, bar_w);

  app_display_clear(&g_disp);
  tw = app_display_text_width(title, scale);
  app_display_text_color(&g_disp, app_center_in(0, g_disp.width, tw), y, title,
                         scale, ARDUBOT_COLOR_TEXT);
  app_display_fill_rect_color(&g_disp, bar_x, y + line_h + gap, bar_w, bar_h, 0,
                              ARDUBOT_COLOR_TRACK);
  if (fill > 0) {
    app_display_fill_rect_color(&g_disp, bar_x, y + line_h + gap, fill, bar_h,
                                0, ARDUBOT_COLOR_ACCENT_COOL);
  }
  tw = app_display_text_width(status, scale);
  app_display_text_color(&g_disp, app_center_in(0, g_disp.width, tw),
                         y + line_h + gap + bar_h + gap, status, scale,
                         ARDUBOT_COLOR_TEXT_MUTED);
  app_display_flush(&g_disp);
#if !defined(ARDUBOT_TARGET_ESP32)
  sim_video_render();
#endif
}

void boot_splash_close(void) { g_open = false; }
