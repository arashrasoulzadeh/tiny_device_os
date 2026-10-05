#pragma once

#include "app_types.h"
#include "ardubot_keys.h"
#include "sim_gpio.h"
#include "ssd1306_model.h"
#include <string.h>
#include "app_kit.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RGB565 packing helper — public so apps (e.g. pong_app.c) can build their
 * own colors for app_ui_pixel_color()/app_ui_rect_color() without
 * duplicating the bit math. */
#define APP_UI_RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

typedef struct {
    struct app_ctx ctx;  // Embed app_ctx_t as first member for compatibility with key callbacks
    app_ui_config_t ui;  // UI configuration
    void* user_data;
    input_recognizer_t* gestures;  // lazily created by the first app_ui_bind_gesture() call
    sim_key_t gesture_gpio_keys[APP_KIT_MAX_KEYS];  // keys that already have GPIO wiring
    int gesture_gpio_key_count;
} app_ui_t;

static inline void app_ui_config_ui(app_ui_config_t* cfg, const char* title, const char* help) {
    cfg->mode = APP_UI_MODE_UI;
    strncpy(cfg->title, title ? title : "", sizeof(cfg->title) - 1);
    cfg->title[sizeof(cfg->title) - 1] = '\0';
    strncpy(cfg->help_text, help ? help : "", sizeof(cfg->help_text) - 1);
    cfg->help_text[sizeof(cfg->help_text) - 1] = '\0';
    /* Used to be forced off: the top bar filled its row with "on" pixels
     * then drew the title with the same "on" pixels - invisible text on
     * its own background on a monochrome display. app_ui_begin_frame() now
     * draws a real colored bar + contrasting title text (see
     * APP_UI_COLOR_TITLE_BG/_TEXT in app_ui.c), so this is safe to enable. */
    cfg->show_top_bar = true;
    cfg->show_help_bar = true;
    cfg->text_scale = (APP_DISPLAY_HEIGHT > 64) ? 2 : 1;
    /* Left/right content margin, device-configurable (see
     * apps/ui/components/display.h's ARDUBOT_UI_PADDING /
     * device_config_esp32c6.yaml's app_kit.ui_padding) - content_x/
     * content_w were hardcoded 0/full-width (no margin at all),
     * invisible on the sim's SDL window but visibly flush-left on a
     * real physical panel (confirmed on hardware, RISCV_TODO.md
     * Phase 4). */
    cfg->content_x = ARDUBOT_UI_PADDING;
    /* content_y must clear the top bar's own height (8*text_scale, same
     * formula app_ui_begin_frame() uses for bar_h) - content_y was left at
     * 0 when the top bar was re-enabled, which put every app's row 0
     * directly under/behind the title bar instead of below it. */
    cfg->content_y = 8 * cfg->text_scale;
    cfg->content_w = APP_DISPLAY_WIDTH - 2 * cfg->content_x;
    /* Must match app_ui_end_frame()'s help_y = HEIGHT - 8*text_scale, or
     * the last content row overlaps the help bar it draws over - and now
     * also subtract the top bar's height reserved via content_y above. */
    cfg->content_h = APP_DISPLAY_HEIGHT - 8 * cfg->text_scale - cfg->content_y;
}

static inline void app_ui_config_game(app_ui_config_t* cfg) {
    cfg->mode = APP_UI_MODE_GAME;
    cfg->title[0] = '\0';
    cfg->help_text[0] = '\0';
    cfg->show_top_bar = false;
    cfg->show_help_bar = false;
    cfg->content_x = 0;
    cfg->content_y = 0;
    cfg->content_w = APP_DISPLAY_WIDTH;
    cfg->content_h = APP_DISPLAY_HEIGHT;
    cfg->text_scale = (APP_DISPLAY_HEIGHT > 64) ? 2 : 1;
}

int app_ui_init(app_ui_t* app, void* real_app, const app_ui_config_t* cfg);
void app_ui_deinit(app_ui_t* app);
int app_ui_bind_key(app_ui_t* app, sim_key_t key, app_key_fn_t fn, void* user);
void app_ui_clear(app_ui_t* app);
void app_ui_begin_frame(app_ui_t* app);
void app_ui_end_frame(app_ui_t* app);
void app_ui_text(app_ui_t* app, int x, int y, const char* text);
void app_ui_text_color(app_ui_t* app, int x, int y, const char* text, uint16_t rgb565);
void app_ui_textf(app_ui_t* app, int x, int y, const char* fmt, ...);
void app_ui_pixel(app_ui_t* app, int x, int y, bool on);
void app_ui_rect(app_ui_t* app, int x, int y, int w, int h, bool fill);
void app_ui_pixel_color(app_ui_t* app, int x, int y, uint16_t rgb565);
void app_ui_rect_color(app_ui_t* app, int x, int y, int w, int h, int radius, uint16_t rgb565);

/* Reusable components (apps/app_ui.c) - every app_ui_t-based stdapp
 * (not games) should compose from these instead of re-deriving the
 * same layout/content-offset math per app. See apps/app_ui.c's own
 * comment above their definitions for the full rationale. */
void app_ui_big_text(app_ui_t* app, int y, const char* text, int scale, uint16_t rgb565);
void app_ui_bar(app_ui_t* app, int y, int h, int fill_w, uint16_t track_color,
                uint16_t fill_color);
void app_ui_bar_centered(app_ui_t* app, int y, int h, int fill_px_signed, uint16_t track_color,
                         uint16_t fill_color, uint16_t tick_color);
void app_ui_panel(app_ui_t* app, int x, int y, int w, int h, const char* label, int label_scale,
                  uint16_t bg_color, uint16_t text_color);

/* Row-based text: row 0 is the top content line, row 1 the next, etc. -
 * the row height always matches app->ui.text_scale, so apps never hardcode
 * a *_scale pixel offset themselves (that was a recurring bug: every stdapp
 * had its own hand-computed "8*scale"/"18*scale" line math). */
static inline int app_ui_row_h(const app_ui_t* app) {
    return 8 * app->ui.text_scale;
}
void app_ui_line(app_ui_t* app, int row, const char* text);
void app_ui_linef(app_ui_t* app, int row, const char* fmt, ...);

/* Binds a whole table of keys in one call: {SIM_KEY_UP, on_up, NULL}, ...
 * terminated by a {0, NULL, NULL} sentinel (key 0 is never a real binding).
 * Returns the number of keys bound, or -1 on the first failure. */
typedef struct {
    sim_key_t key;
    app_key_fn_t fn;
    void* user;
} app_ui_key_def_t;
int app_ui_bind_keys(app_ui_t* app, const app_ui_key_def_t* defs);

/* Gesture binding: SIM_KEY_* -> a real tap/long-press/double-tap/hold
 * classification (apps/input.c), instead of the plain press-only firing
 * app_ui_bind_key() gives you. `gesture` is one of input.h's
 * INPUT_EVENT_BUTTON_* (or the touch/encoder ones, for devices that have
 * them). Lazily initializes this app_ui_t's own input_recognizer_t and
 * GPIO edge wiring on first use; call it instead of app_ui_bind_key() for
 * a given key, not alongside it - both would register their own GPIO
 * pin for the same sim_key_t. Returns 0, or -1 on failure. */
int app_ui_bind_gesture(app_ui_t* app, sim_key_t key, input_event_type_t gesture,
                         input_callback_t cb, void* user);

void app_ui_mark_dirty(app_ui_t* app);
void app_ui_clear_dirty(app_ui_t* app);
bool app_ui_is_dirty(const app_ui_t* app);
void app_ui_menu_nav_next(void* app, void* user);
void app_ui_menu_nav_prev(void* app, void* user);

#ifdef __cplusplus
}
#endif
