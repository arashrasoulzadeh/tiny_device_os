#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef APP_CTX_T_DECLARED
#define APP_CTX_T_DECLARED
typedef struct app_ctx app_ctx_t;
#endif

void app_mark_dirty(app_ctx_t* app);
void app_clear_dirty(app_ctx_t* app);
bool app_is_dirty(const app_ctx_t* app);

void app_clear(app_ctx_t* app);
void app_text(app_ctx_t* app, int x, int y, const char* text);
void app_textf(app_ctx_t* app, int x, int y, const char* fmt, ...);
void app_pixel(app_ctx_t* app, int x, int y, bool on);
void app_flush(app_ctx_t* app);

/* Color (RGB565) drawing — for the card-style color launcher (menu.c). A
 * no-op on a plain mono display (app_display_* color wrappers only act
 * when the sim's color model is backing the display; real mono hardware
 * builds simply never call these). */
void app_pixel_color(app_ctx_t* app, int x, int y, uint16_t rgb565);
void app_fill_rect_color(app_ctx_t* app, int x, int y, int w, int h, int radius, uint16_t rgb565);
void app_draw_rect_color(app_ctx_t* app, int x, int y, int w, int h, int radius, uint16_t rgb565);
void app_fill_circle_color(app_ctx_t* app, int cx, int cy, int r, uint16_t rgb565);
void app_hline_color(app_ctx_t* app, int x, int y, int w, uint16_t rgb565);
void app_text_color(app_ctx_t* app, int x, int y, const char* text, int scale, uint16_t rgb565);
int app_text_width(const char* text, int scale);

#ifdef __cplusplus
}
#endif
