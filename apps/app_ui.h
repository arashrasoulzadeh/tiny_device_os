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
    cfg->show_top_bar = true;
    cfg->show_help_bar = true;
    cfg->content_x = 0;
    cfg->content_y = 10;
    cfg->content_w = APP_DISPLAY_WIDTH;
    cfg->content_h = APP_DISPLAY_HEIGHT - 18;
    cfg->text_scale = (APP_DISPLAY_HEIGHT > 64) ? 2 : 1;
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

int app_ui_init(app_ui_t* app, const app_ui_config_t* cfg);
void app_ui_deinit(app_ui_t* app);
int app_ui_bind_key(app_ui_t* app, sim_key_t key, app_key_fn_t fn, void* user);
void app_ui_clear(app_ui_t* app);
void app_ui_begin_frame(app_ui_t* app);
void app_ui_end_frame(app_ui_t* app);
void app_ui_text(app_ui_t* app, int x, int y, const char* text);
void app_ui_textf(app_ui_t* app, int x, int y, const char* fmt, ...);
void app_ui_pixel(app_ui_t* app, int x, int y, bool on);
void app_ui_rect(app_ui_t* app, int x, int y, int w, int h, bool fill);

void app_ui_mark_dirty(app_ui_t* app);
void app_ui_clear_dirty(app_ui_t* app);
bool app_ui_is_dirty(const app_ui_t* app);
void app_ui_menu_nav_next(void* app, void* user);
void app_ui_menu_nav_prev(void* app, void* user);

#ifdef __cplusplus
}
#endif
