#include "app_ui.h"
#include "app_framework.h"
#include "app_kit.h"
#include "fw/io.h"
#include "fw/ui.h"
#include "header_app.h"
#include "menu.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>

extern int g_next_pin;

/* Shared color theme for every app_ui_t-based app (counter/info/stopwatch/
 * pong/settings/shell/widgets/demo/fileman all go through this one file) —
 * see ssd1306_model.c for why the color API lives in a file named after a
 * mono chip. APP_UI_RGB565 itself is declared in app_ui.h so other apps
 * (e.g. pong_app.c) can build their own colors with it. Values come from
 * apps/ui/components/theme.h's shared design tokens, not local literals -
 * every app_ui_t-based app's title/help bar now matches every other one
 * by construction. */
#define APP_UI_COLOR_TITLE_BG ARDUBOT_COLOR_TITLE_BG
#define APP_UI_COLOR_TITLE_TEXT ARDUBOT_COLOR_TITLE_TEXT
#define APP_UI_COLOR_TEXT ARDUBOT_COLOR_TEXT
#define APP_UI_COLOR_HELP ARDUBOT_COLOR_HELP_TEXT

int app_ui_init(app_ui_t *app, void *real_app, const app_ui_config_t *cfg) {
  if (!app || !cfg)
    return -1;

  memset(app, 0, sizeof(*app));
  if (real_app) {
    /* The kit context already owns the display. Copying the layout
     * onto it is the whole job; a second app_display_init() would
     * wipe the panel. */
    app->ctx = (app_ctx_t *)real_app;
    app->owns_ctx = false;
  } else {
    app->ctx = calloc(1, sizeof(*app->ctx));
    if (!app->ctx)
      return -1;
    app->owns_ctx = true;
    if (app_display_init(&app->ctx->display, "/dev/display0") != 0) {
      free(app->ctx);
      app->ctx = NULL;
      return -1;
    }
    app_timer_init(&app->ctx->timer, 30);
  }
  app->ctx->ui = *cfg;
  return 0;
}

void app_ui_deinit(app_ui_t *app) {
  if (!app)
    return;
  if (app->gestures) {
    input_recognizer_destroy(app->gestures);
    app->gestures = NULL;
  }
  if (app->owns_ctx && app->ctx) {
    app_display_deinit(&app->ctx->display);
    free(app->ctx);
    app->ctx = NULL;
    app->owns_ctx = false;
  }
}

static void ui_key_trampoline(int pin, void *arg) {
  app_key_binding_t *binding = (app_key_binding_t *)arg;
  (void)pin;
  if (!binding || !binding->fn || !binding->app) {
    return;
  }
  /* Key bindings are never torn down when an app is merely suspended
   * (just backgrounded via app_open()/app_request_exit(), not fully
   * exited) - its pins stay registered. Without this check, every key
   * press reaches every app that ever bound that key, not just the
   * focused one: navigating inside a later-opened app also silently
   * drives whichever background app is still listening (e.g. the
   * launcher's own menu selection), and a later Enter press fires that
   * background app's handler too - which is how opening an unrelated
   * app out of nowhere, or two apps' frames alternating on screen, kept
   * happening. kit_key_trampoline() in app_kit.c already guards this the
   * same way; this is the same mechanism's other binding path. */
  /* Bindings live on the kit context, the same object app_kit_run()
   * focuses, so the foreground check is the context pointer. */
  if (!app_kit_is_foreground(binding->app)) {
    return;
  }
  binding->fn((app_ctx_t *)binding->app, binding->user);
}

int app_ui_bind_key(app_ui_t *app, sim_key_t key, app_key_fn_t fn, void *user) {
  if (!app || !fn)
    return -1;
  if (app->ctx->key_count >= APP_KIT_MAX_KEYS)
    return -1;

  app_key_binding_t *binding = &app->ctx->keys[app->ctx->key_count];
  binding->key = key;
  binding->fn = fn;
  binding->user = user;
  binding->app = app->ctx;

  int pin = g_next_pin++;
  app_button_t btn = APP_KEY_BUTTON(pin, key, ui_key_trampoline, NULL, binding);
  if (app_button_init(&btn) != 0)
    return -1;

  sim_gpio_set_key_mapping(key, pin, true);

  app->ctx->key_count++;
  return 0;
}

static input_key_t sim_key_to_input_key(sim_key_t key) {
  switch (key) {
  case SIM_KEY_UP:
    return INPUT_KEY_UP;
  case SIM_KEY_DOWN:
    return INPUT_KEY_DOWN;
  case SIM_KEY_LEFT:
    return INPUT_KEY_LEFT;
  case SIM_KEY_RIGHT:
    return INPUT_KEY_RIGHT;
  case SIM_KEY_ENTER:
    return INPUT_KEY_ENTER;
  case SIM_KEY_ESCAPE:
    return INPUT_KEY_ESCAPE;
  case SIM_KEY_SPACE:
    return INPUT_KEY_SPACE;
  case SIM_KEY_A:
    return INPUT_KEY_A;
  case SIM_KEY_B:
    return INPUT_KEY_B;
  case SIM_KEY_C:
    return INPUT_KEY_C;
  case SIM_KEY_D:
    return INPUT_KEY_D;
  default:
    return INPUT_KEY_UNKNOWN;
  }
}

typedef struct {
  app_ui_t *app;
  sim_key_t key;
} gesture_binding_t;

/* Bridges apps/input.c's global device-callback dispatch to this app_ui_t's
 * own recognizer - input_process_events() delivers to devices, not
 * directly to a recognizer, so every app_ui_t with a gesture binding
 * registers one small device whose only job is this forward. */
static void gesture_device_cb(const input_event_t *event, void *arg) {
  app_ui_t *app = (app_ui_t *)arg;
  input_recognizer_dispatch(app->gestures, event);
}

/* Single trampoline for both edges of a gesture-bound key (HAL_GPIO_IRQ_BOTH -
 * app_ui_bind_key()'s own trampoline is press-only on purpose, see the
 * comment on APP_KEY_BUTTON; gestures need both edges to tell a tap from a
 * hold, so this one reads the pin itself instead of relying on separate
 * press/release callback slots). */
static void gesture_key_trampoline(int pin, void *arg) {
  gesture_binding_t *gb = (gesture_binding_t *)arg;
  if (!gb || !gb->app)
    return;
  if (!gb->app->ctx || !app_kit_is_foreground_desc(gb->app->ctx->desc))
    return;

  bool pressed = sim_gpio_read(pin);
  input_event_t ev = {0};
  ev.type = pressed ? INPUT_EVENT_KEY_DOWN : INPUT_EVENT_KEY_UP;
  ev.key = sim_key_to_input_key(gb->key);
  input_post_event(&ev);
  input_process_events();
}

int app_ui_bind_gesture(app_ui_t *app, sim_key_t key,
                        input_event_type_t gesture, input_callback_t cb,
                        void *user) {
  if (!app || !cb)
    return -1;

  if (!app->gestures) {
    app->gestures = input_recognizer_create();
    if (!app->gestures)
      return -1;

    char devname[32];
    snprintf(devname, sizeof(devname), "gestures_%p", (void *)app);
    if (input_device_register(INPUT_DEV_BUTTONS, devname, gesture_device_cb,
                              app) != 0) {
      input_recognizer_destroy(app->gestures);
      app->gestures = NULL;
      return -1;
    }
  }

  if (input_recognizer_add_gesture(app->gestures, gesture, cb, user) != 0)
    return -1;

  /* One recognizer already fans out to every gesture type registered
   * for it (a single posted event only matches entries whose gesture
   * equals its type), so binding e.g. both TAP and LONG_TAP on the same
   * key only needs ONE underlying GPIO pin/trampoline - a second one
   * would make sim_gpio_handle_key() (which fires every pin mapped to a
   * given key) run the trampoline, and so post+process the same key
   * event, twice per physical press. */
  for (int i = 0; i < app->gesture_gpio_key_count; i++) {
    if (app->gesture_gpio_keys[i] == key)
      return 0;
  }

  gesture_binding_t *gb = calloc(1, sizeof(gesture_binding_t));
  if (!gb)
    return -1;
  gb->app = app;
  gb->key = key;

  int pin = g_next_pin++;
  app_button_t btn = {
      .pin = pin,
      .key = key,
      .trigger = HAL_GPIO_IRQ_BOTH,
      .on_press = gesture_key_trampoline,
      .on_release = NULL,
      .arg = gb,
  };
  if (app_button_init(&btn) != 0) {
    free(gb);
    return -1;
  }
  sim_gpio_set_key_mapping(key, pin, true);

  if (app->gesture_gpio_key_count < APP_KIT_MAX_KEYS) {
    app->gesture_gpio_keys[app->gesture_gpio_key_count++] = key;
  }

  return 0;
}

void app_ui_clear(app_ui_t *app) { app_display_clear(&app->ctx->display); }

void app_ui_begin_frame(app_ui_t *app) {
  /* Set before the app draws. The launcher flushes from its own draw,
   * and that flush reads the band from header_app.c. */
  header_app_set_visible(app->ctx->ui.mode == APP_UI_MODE_UI);
  app_display_clear(&app->ctx->display);

  if (app->ctx->ui.mode == APP_UI_MODE_UI && app->ctx->ui.show_top_bar &&
      app->ctx->ui.title[0]) {
    /* Colored title bar + contrasting text — on mono this used to be
     * "on" bar + "on" text (invisible), which is why show_top_bar
     * defaulted to false (see app_ui_config_ui()'s comment). With a
     * real background/foreground color pair this actually reads. */
    int bar_h = 8 * app->ctx->ui.text_scale;
    int header = app_header_height();
    /* The bar background intentionally spans the full width (a
     * colored bar with a visible side margin looks like a mistake,
     * not padding) - only the title text itself gets the same left
     * margin as body content, for visual consistency with it.
     * It starts under the launcher header so the two bands do not
     * share a row. */
    app_display_fill_rect_color(&app->ctx->display, 0, header,
                                APP_DISPLAY_WIDTH, bar_h, 0,
                                APP_UI_COLOR_TITLE_BG);
    app_display_text_color(&app->ctx->display, 2 + app->ctx->ui.content_x,
                           header, app->ctx->ui.title, app->ctx->ui.text_scale,
                           APP_UI_COLOR_TITLE_TEXT);
  }
}

void app_ui_end_frame(app_ui_t *app) {
  if (app->ctx->ui.mode == APP_UI_MODE_UI && app->ctx->ui.show_help_bar &&
      app->ctx->ui.help_text[0]) {
    int help_y = APP_DISPLAY_HEIGHT - 8 * app->ctx->ui.text_scale;
    app_display_text_color(&app->ctx->display, app->ctx->ui.content_x, help_y,
                           app->ctx->ui.help_text, app->ctx->ui.text_scale,
                           APP_UI_COLOR_HELP);
  }
  /* Flush reads the band from header_app.c when this is a standard
   * screen. Games and fullscreen left it hidden in begin_frame. */
  header_app_set_visible(app->ctx->ui.mode == APP_UI_MODE_UI);
  app_display_flush(&app->ctx->display);
}

void app_ui_text(app_ui_t *app, int x, int y, const char *text) {
  app_ui_text_color(app, x, y, text, APP_UI_COLOR_TEXT);
}

void app_ui_text_color(app_ui_t *app, int x, int y, const char *text,
                       uint16_t rgb565) {
  if (!text || !app)
    return;
  // Validate pointer is in valid user space (not kernel space)
  if ((uintptr_t)text > 0x7FFFFFFFFFFF)
    return;
  int draw_y = y + app->ctx->ui.content_y;
  int draw_x = x + app->ctx->ui.content_x;
  if (draw_y >= 0 && draw_y < app->ctx->ui.content_h) {
    app_display_text_color(&app->ctx->display, draw_x, draw_y, text,
                           app->ctx->ui.text_scale, rgb565);
  }
}

void app_ui_textf(app_ui_t *app, int x, int y, const char *fmt, ...) {
  if (!fmt || !app)
    return;
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  app_ui_text(app, x, y, buf);
}

void app_ui_line(app_ui_t *app, int row, const char *text) {
  app_ui_text(app, 0, row * app_ui_row_h(app), text);
}

void app_ui_linef(app_ui_t *app, int row, const char *fmt, ...) {
  if (!fmt || !app)
    return;
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  app_ui_line(app, row, buf);
}

int app_ui_bind_keys(app_ui_t *app, const app_ui_key_def_t *defs) {
  if (!app || !defs)
    return -1;
  int n = 0;
  for (; defs[n].fn != NULL; n++) {
    if (app_ui_bind_key(app, defs[n].key, defs[n].fn, defs[n].user) != 0) {
      return -1;
    }
  }
  return n;
}

void app_ui_pixel(app_ui_t *app, int x, int y, bool on) {
  int draw_y = y + app->ctx->ui.content_y;
  if (draw_y >= 0 && draw_y < app->ctx->ui.content_h) {
    app_display_pixel(&app->ctx->display, x, draw_y, on);
  }
}

void app_ui_rect(app_ui_t *app, int x, int y, int w, int h, bool fill) {
  int draw_y = y + app->ctx->ui.content_y;
  if (draw_y >= 0 && draw_y + h <= app->ctx->ui.content_h) {
    app_display_rect(&app->ctx->display, x, draw_y, w, h, fill);
  }
}

void app_ui_pixel_color(app_ui_t *app, int x, int y, uint16_t rgb565) {
  int draw_y = y + app->ctx->ui.content_y;
  if (draw_y >= 0 && draw_y < app->ctx->ui.content_h) {
    app_display_pixel_color(&app->ctx->display, x, draw_y, rgb565);
  }
}

void app_ui_rect_color(app_ui_t *app, int x, int y, int w, int h, int radius,
                       uint16_t rgb565) {
  int draw_y = y + app->ctx->ui.content_y;
  if (draw_y >= 0 && draw_y + h <= app->ctx->ui.content_h) {
    app_display_fill_rect_color(&app->ctx->display, x, draw_y, w, h, radius,
                                rgb565);
  }
}

/* ===========================================================================
 * Reusable components - built from the patterns repeated across
 * counter_app.c/pomodoro_app.c/info_app.c (a big centered stat number, a
 * depleting/filling bar, a colored label panel). Every app_ui_t-based
 * stdapp (not games - app_ui_config_game() apps draw their own playfield
 * directly) should compose screens from these plus app_ui_line()/
 * app_ui_text() rather than re-deriving the same layout math and
 * content_x/content_y offsetting per app, the way pomodoro_app.c and
 * counter_app.c both independently did before this existed. Takes
 * content-relative x (like app_ui_rect_color() already does for y) and
 * handles the content_x offset itself, unlike app_ui_rect_color(), whose
 * x param is NOT content_x-offset - a pre-existing inconsistency not
 * touched here to avoid changing that function's existing call sites.
 * ========================================================================= */

/* Centered stat number/word at an arbitrary scale - app_ui_text_color()
 * is always this app's fixed body text_scale, too small to read as a
 * headline element (e.g. a countdown or counter's current value). */
void app_ui_big_text(app_ui_t *app, int y, const char *text, int scale,
                     uint16_t rgb565) {
  if (!app || !text || scale <= 0)
    return;
  int w = app_display_text_width(text, scale);
  int x = app->ctx->ui.content_x + (app->ctx->ui.content_w - w) / 2;
  int draw_y = y + app->ctx->ui.content_y;
  if (draw_y >= 0 && draw_y + 7 * scale <= app->ctx->ui.content_h) {
    app_display_text_color(&app->ctx->display, x, draw_y, text, scale, rgb565);
  }
}

/* Left-to-right depleting/filling bar spanning the full content width
 * (e.g. a countdown's remaining-time bar). fill_w is clamped to
 * [0, content_w] - callers don't need to clamp it themselves. */
void app_ui_bar(app_ui_t *app, int y, int h, int fill_w, uint16_t track_color,
                uint16_t fill_color) {
  if (!app)
    return;
  int w = app->ctx->ui.content_w;
  int draw_y = y + app->ctx->ui.content_y;
  if (draw_y < 0 || draw_y + h > app->ctx->ui.content_h)
    return;
  if (fill_w < 0)
    fill_w = 0;
  if (fill_w > w)
    fill_w = w;
  app_display_fill_rect_color(&app->ctx->display, app->ctx->ui.content_x,
                              draw_y, w, h, 0, track_color);
  if (fill_w > 0) {
    app_display_fill_rect_color(&app->ctx->display, app->ctx->ui.content_x,
                                draw_y, fill_w, h, 0, fill_color);
  }
}

/* Bidirectional bar growing from a center tick (e.g. a signed value's
 * gauge) - fill_px_signed extends right of center when positive, left
 * when negative; the caller computes and clamps it (the right clamp
 * range is value-semantics the component shouldn't need to know). */
void app_ui_bar_centered(app_ui_t *app, int y, int h, int fill_px_signed,
                         uint16_t track_color, uint16_t fill_color,
                         uint16_t tick_color) {
  if (!app)
    return;
  int w = app->ctx->ui.content_w;
  int draw_y = y + app->ctx->ui.content_y;
  if (draw_y < 0 || draw_y + h > app->ctx->ui.content_h)
    return;
  int center = app->ctx->ui.content_x + w / 2;
  app_display_fill_rect_color(&app->ctx->display, app->ctx->ui.content_x,
                              draw_y, w, h, 0, track_color);
  if (fill_px_signed > 0) {
    app_display_fill_rect_color(&app->ctx->display, center, draw_y,
                                fill_px_signed, h, 0, fill_color);
  } else if (fill_px_signed < 0) {
    app_display_fill_rect_color(&app->ctx->display, center + fill_px_signed,
                                draw_y, -fill_px_signed, h, 0, fill_color);
  }
  app_display_fill_rect_color(&app->ctx->display, center - 1, draw_y, 2, h, 0,
                              tick_color);
}

/* Solid-color panel with a centered label (e.g. a status block like
 * pomodoro's "WORK"/"REST" indicator). x/w are content-relative, same as
 * every other coordinate in this component set. */
void app_ui_panel(app_ui_t *app, int x, int y, int w, int h, const char *label,
                  int label_scale, uint16_t bg_color, uint16_t text_color) {
  if (!app)
    return;
  int draw_y = y + app->ctx->ui.content_y;
  if (draw_y < 0 || draw_y + h > app->ctx->ui.content_h)
    return;
  int abs_x = app->ctx->ui.content_x + x;
  app_display_fill_rect_color(&app->ctx->display, abs_x, draw_y, w, h, 0,
                              bg_color);
  if (label && label_scale > 0) {
    int label_w = app_display_text_width(label, label_scale);
    int label_h = 7 * label_scale;
    app_display_text_color(&app->ctx->display, abs_x + (w - label_w) / 2,
                           draw_y + (h - label_h) / 2, label, label_scale,
                           text_color);
  }
}

void app_ui_mark_dirty(app_ui_t *app) {
  if (app) {
    app->ctx->dirty = true;
  }
}

void app_ui_clear_dirty(app_ui_t *app) {
  if (app) {
    app->ctx->dirty = false;
  }
}

bool app_ui_is_dirty(const app_ui_t *app) {
  return app ? app->ctx->dirty : false;
}

static void app_ui_menu_move(app_ui_t *app, app_menu_t *menu, int delta,
                             void *user) {
  (void)user;
  if (!app || !menu || delta == 0) {
    return;
  }

  int visible;
  int next;

  if (menu->count <= 0 || delta == 0) {
    return;
  }

  next = menu->selected + delta;
  while (next < 0) {
    next += menu->count;
  }
  while (next >= menu->count) {
    next -= menu->count;
  }
  if (next == menu->selected) {
    return;
  }

  menu->selected = next;

  if (menu->layout != APP_MENU_LAYOUT_ICONS) {
    int avail = app->ctx->ui.content_h - menu->start_y - 8;
    int rows = avail / menu->row_h;
    if (rows < 0)
      rows = 0;
    if (rows > menu->count)
      rows = menu->count;
    visible = rows;
    if (visible > 0) {
      if (menu->selected < menu->first_visible) {
        menu->first_visible = menu->selected;
      } else if (menu->selected >= menu->first_visible + visible) {
        menu->first_visible = menu->selected - visible + 1;
      }
    }
  }

  app_ui_mark_dirty(app);
}

void app_ui_menu_nav_next(void *app, void *user) {
  app_ui_menu_move((app_ui_t *)app, (app_menu_t *)user, APP_MENU_ONE_DOWN,
                   user);
}

void app_ui_menu_nav_prev(void *app, void *user) {
  app_ui_menu_move((app_ui_t *)app, (app_menu_t *)user, APP_MENU_ONE_UP, user);
}
