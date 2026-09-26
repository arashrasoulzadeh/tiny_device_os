#pragma once

#include <stdarg.h>
#include <stdbool.h>

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
void app_flush(app_ctx_t* app);

#ifdef __cplusplus
}
#endif
