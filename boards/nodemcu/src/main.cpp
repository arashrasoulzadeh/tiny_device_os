/**
 * ArdubotOS NodeMCU board entry (PlatformIO + Arduino).
 *
 * Boots the same builtin apps as the host simulator (launcher → counter /
 * info / stopwatch / pong) on the SSD1306, driven by two push-buttons:
 *   UP     — navigate (wrap) / app actions
 *   SELECT — launch / confirm / (long-press = back)
 *
 * Pins come from build/generated/device_config.h (device_config.yaml).
 * Wi-Fi SSID and password come from device_secrets.h (device_secrets.yaml).
 */

#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>

#include "device_config.h"

#if defined(__has_include)
#if __has_include("device_secrets.h")
#include "device_secrets.h"
#endif
#endif
#ifndef ARDUBOT_WIFI_HAS_CREDS
#define ARDUBOT_WIFI_HAS_CREDS 0
#define ARDUBOT_WIFI_SSID ""
#define ARDUBOT_WIFI_PASSWORD ""
#endif

#if defined(ARDUBOT_TARGET_ESP8266) && ARDUBOT_WIFI_HAS_CREDS
#include <ESP8266WiFi.h>
#endif
#include "ssd1306_mini.h"
#include "icons.h"
#include "status.h"
#include "pong.h"

#ifndef ARDUBOT_LCD_WIDTH
#define ARDUBOT_LCD_WIDTH 128
#endif
#ifndef ARDUBOT_LCD_HEIGHT
#define ARDUBOT_LCD_HEIGHT 32
#endif
#ifndef ARDUBOT_LCD_SCL_GPIO
#define ARDUBOT_LCD_SCL_GPIO 5
#endif
#ifndef ARDUBOT_LCD_SDA_GPIO
#define ARDUBOT_LCD_SDA_GPIO 4
#endif
#ifndef ARDUBOT_LCD_I2C_ADDR
#define ARDUBOT_LCD_I2C_ADDR 0x3C
#endif
#ifndef ARDUBOT_BTN_UP_GPIO
#define ARDUBOT_BTN_UP_GPIO 14
#endif
#ifndef ARDUBOT_BTN_SELECT_GPIO
#define ARDUBOT_BTN_SELECT_GPIO 12
#endif
#ifndef ARDUBOT_BTN_UP_ACTIVE_LOW
#define ARDUBOT_BTN_UP_ACTIVE_LOW 1
#endif
#ifndef ARDUBOT_BTN_SELECT_ACTIVE_LOW
#define ARDUBOT_BTN_SELECT_ACTIVE_LOW 1
#endif

#define LONG_PRESS_MS 700

enum AppId : uint8_t { APP_LAUNCHER = 0, APP_COUNTER, APP_INFO, APP_STOPWATCH, APP_PONG, APP_COUNT };

struct MenuItem {
  AppId id;
  const char* name;
  const char* tag;
  const app_icon_t* icon;
};

extern const app_icon_t counter_app_icon;
extern const app_icon_t info_app_icon;
extern const app_icon_t stopwatch_app_icon;
extern const app_icon_t pong_app_icon;

static const MenuItem k_apps[] = {
    {APP_COUNTER, "counter", "[USR]", &counter_app_icon},
    {APP_INFO, "info", "[TOL]", &info_app_icon},
    {APP_STOPWATCH, "stopwatch", "[TOL]", &stopwatch_app_icon},
    {APP_PONG, "pong", "[GME]", &pong_app_icon},
};
static const int k_app_count = (int)(sizeof(k_apps) / sizeof(k_apps[0]));

static Ssd1306 display(ARDUBOT_LCD_WIDTH, ARDUBOT_LCD_HEIGHT, (uint8_t)ARDUBOT_LCD_I2C_ADDR);

static AppId g_app = APP_LAUNCHER;
static int g_sel = 0;
static int32_t g_count = 0;
static uint32_t g_boot_ms = 0;

static bool g_sw_running = false;
static uint8_t g_sw_h = 0, g_sw_m = 0, g_sw_s = 0;
static uint32_t g_sw_last_ms = 0;

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

static void go_home(void) {
  g_app = APP_LAUNCHER;
  mark_dirty();
}

static void open_app(AppId id) {
  g_app = id;
  if (id == APP_COUNTER) {
    /* keep count */
  } else if (id == APP_STOPWATCH) {
    /* keep stopwatch state */
  } else if (id == APP_PONG) {
    pong_reset(&g_pong);
  }
  mark_dirty();
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
  Ssd1306* d = (Ssd1306*)user;
  if (d && on) {
    d->set_pixel(px, py, true);
  }
}

static app_status_link_t g_link = APP_STATUS_LINK_OFF;
static int g_rssi = -127;

#if defined(ARDUBOT_TARGET_ESP8266) && ARDUBOT_WIFI_HAS_CREDS
static void poll_wifi(void) {
  static int shown = 0;
  static int pending = 0;
  static uint32_t pending_since = 0;
  const bool up = WiFi.status() == WL_CONNECTED;
  const int rssi = up ? (int)WiFi.RSSI() : -127;
  const app_status_link_t link = up ? APP_STATUS_LINK_UP : APP_STATUS_LINK_DOWN;
  const int proposed = app_status_signal_bars(link, rssi);
  const int next = app_status_stable_bars(shown, proposed, millis(), &pending, &pending_since);
  if (next != shown) {
    shown = next;
    g_link = link;
    g_rssi = rssi;
    mark_dirty();
  }
}
#else
static void poll_wifi(void) {}
#endif

static void draw_status(void) {
  app_status_blit(ARDUBOT_LCD_WIDTH, app_status_battery_percent(), g_link, g_rssi, board_set_pixel,
                  &display);
}

static void draw_launcher(void) {
  const int icon_y = 8;
  const int label_y = 24;
  const int center_x = (ARDUBOT_LCD_WIDTH / 2) - (APP_ICON_SIZE / 2);
  int side = ((ARDUBOT_LCD_WIDTH - APP_STATUS_WIDTH) / APP_ICON_PITCH) / 2;
  int off;
  int len;
  int lx;
  const char* name;

  if (side < 1) {
    side = 1;
  }

  display.clear();
  for (off = -side; off <= side; off++) {
    int idx = g_sel + off;
    int x;
    while (idx < 0) {
      idx += k_app_count;
    }
    while (idx >= k_app_count) {
      idx -= k_app_count;
    }
    x = center_x + off * APP_ICON_PITCH;
    if (x + APP_ICON_SIZE > ARDUBOT_LCD_WIDTH - APP_STATUS_WIDTH && off != 0) {
      continue;
    }
    app_icon_blit(x, icon_y, k_apps[idx].icon, off == 0, board_set_pixel, &display);
  }
  name = k_apps[g_sel].name;
  len = (int)strlen(name);
  lx = (ARDUBOT_LCD_WIDTH - len * 6) / 2;
  if (lx < 0) {
    lx = 0;
  }
  display.draw_text(lx, label_y, name);
  draw_status();
  display.display();
}

static void draw_counter(void) {
  char line[22];
  display.clear();
  display.draw_text(0, 0, "Counter");
  snprintf(line, sizeof(line), "Count: %ld", (long)g_count);
  display.draw_text(0, 8, line);
  display.draw_text(0, 16, "Up:+  Sel:-");
  display.draw_text(0, 24, "hold Sel: back");
  draw_status();
  display.display();
}

static void draw_info(void) {
  char line[22];
  const uint32_t up_s = (millis() - g_boot_ms) / 1000u;
  display.clear();
  display.draw_text(0, 0, "Device Info");
  display.draw_text(0, 8, "ArdubotOS esp8266");
  snprintf(line, sizeof(line), "Disp %ux%u", (unsigned)ARDUBOT_LCD_WIDTH,
           (unsigned)ARDUBOT_LCD_HEIGHT);
  display.draw_text(0, 16, line);
  snprintf(line, sizeof(line), "Up:%lus Apps:%d", (unsigned long)up_s, k_app_count);
  display.draw_text(0, 24, line);
  draw_status();
  display.display();
}

static void draw_stopwatch(void) {
  char line[22];
  display.clear();
  display.draw_text(0, 0, "Stopwatch");
  snprintf(line, sizeof(line), "%02u:%02u:%02u", (unsigned)g_sw_h, (unsigned)g_sw_m,
           (unsigned)g_sw_s);
  display.draw_text(0, 8, line);
  display.draw_text(0, 16, g_sw_running ? "RUN" : "STP");
  display.draw_text(0, 24, "Up:tog Sel:rst");
  draw_status();
  display.display();
}

static void draw_pong(void) {
  char line[16];
  int y;
  int i;
  display.clear();
  for (y = 0; y < PONG_H && y < ARDUBOT_LCD_HEIGHT; y++) {
    display.set_pixel(PONG_W - 2, y, true);
    display.set_pixel(PONG_W - 1, y, true);
  }
  for (i = 0; i < PONG_PADDLE_H; i++) {
    display.set_pixel(PONG_PADDLE_X, g_pong.paddle_y + i, true);
    display.set_pixel(PONG_PADDLE_X + 1, g_pong.paddle_y + i, true);
  }
  for (y = 0; y < PONG_BALL; y++) {
    for (i = 0; i < PONG_BALL; i++) {
      display.set_pixel(g_pong.ball_x + i, g_pong.ball_y + y, true);
    }
  }
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
  const uint32_t now = millis();

  if (up && !g_up_down) {
    on_up();
  }
  g_up_down = up;

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
  Serial.println(F("ArdubotOS NodeMCU — launcher + builtins"));
  Serial.printf("LCD %dx%d SDA=GPIO%d SCL=GPIO%d\n", ARDUBOT_LCD_WIDTH, ARDUBOT_LCD_HEIGHT,
                ARDUBOT_LCD_SDA_GPIO, ARDUBOT_LCD_SCL_GPIO);
  Serial.printf("BTN up=GPIO%d select=GPIO%d (long-select = back)\n", ARDUBOT_BTN_UP_GPIO,
                ARDUBOT_BTN_SELECT_GPIO);

  pinMode(ARDUBOT_BTN_UP_GPIO, INPUT_PULLUP);
  pinMode(ARDUBOT_BTN_SELECT_GPIO, INPUT_PULLUP);

#if defined(ARDUBOT_TARGET_ESP8266) && ARDUBOT_WIFI_HAS_CREDS
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ARDUBOT_WIFI_SSID, ARDUBOT_WIFI_PASSWORD);
  Serial.printf("WiFi connecting to %s\n", ARDUBOT_WIFI_SSID);
#else
  Serial.println(F("WiFi idle — set ssid and password in device_secrets.yaml"));
#endif

  Wire.begin(ARDUBOT_LCD_SDA_GPIO, ARDUBOT_LCD_SCL_GPIO);
  if (!display.begin()) {
    Serial.println(F("SSD1306 init failed — check wiring / address"));
    for (;;) {
      delay(1000);
    }
  }

  g_boot_ms = millis();
  app_status_set_battery_percent(100); /* USB-powered NodeMCU */
  mark_dirty();
  redraw();
}

void loop() {
  poll_buttons();
  poll_wifi();
  stopwatch_tick();
  pong_tick();
  /* Info uptime refreshes about once a second while visible */
  static uint32_t last_info;
  if (g_app == APP_INFO && millis() - last_info > 1000u) {
    last_info = millis();
    mark_dirty();
  }
  redraw();
  delay(15);
}
