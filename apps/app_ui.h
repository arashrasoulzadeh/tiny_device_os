#pragma once

#include "app_types.h"
#include "ardubot_keys.h"
#include "sim_gpio.h"
#include "ssd1306_model.h"
#include <string.h>
#include "app_kit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct app_ctx ctx;  // Embed app_ctx_t as first member for compatibility with key callbacks
    app_ui_config_t ui;  // UI configuration
    void* user_data;
} app_ui_t;

static inline void app_ui_config_ui(app_ui_config_t* cfg, const char* title, const char* help) {
    cfg->mode = APP_UI_MODE_UI;
    strncpy(cfg->title, title ? title : "", sizeof(cfg->title) - 1);
    cfg->title[sizeof(cfg->title) - 1] = '\0';
    strncpy(cfg->help_text, help ? help : "", sizeof(cfg->help_text) - 1);
    cfg->help_text[sizeof(cfg->help_text) - 1] = '\0';
    /* The top bar fills its row with "on" pixels then draws the title with
     * the same "on" pixels - on this monochrome display that's invisible
     * text on its own background, so it only ever showed as a blank white
     * strip. Left off until it can actually invert (on bg, off text). */
    cfg->show_top_bar = false;
    cfg->show_help_bar = true;
    cfg->text_scale = (APP_DISPLAY_HEIGHT > 64) ? 2 : 1;
    cfg->content_x = 0;
    cfg->content_y = 0;
    cfg->content_w = APP_DISPLAY_WIDTH;
    /* Must match app_ui_end_frame()'s help_y = HEIGHT - 8*text_scale, or
     * the last content row overlaps the help bar it draws over. */
    cfg->content_h = APP_DISPLAY_HEIGHT - 8 * cfg->text_scale;
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
void app_ui_textf(app_ui_t* app, int x, int y, const char* fmt, ...);
void app_ui_pixel(app_ui_t* app, int x, int y, bool on);
void app_ui_rect(app_ui_t* app, int x, int y, int w, int h, bool fill);

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

void app_ui_mark_dirty(app_ui_t* app);
void app_ui_clear_dirty(app_ui_t* app);
bool app_ui_is_dirty(const app_ui_t* app);
void app_ui_menu_nav_next(void* app, void* user);
void app_ui_menu_nav_prev(void* app, void* user);

#ifdef __cplusplus
}
#endif
