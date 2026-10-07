#pragma once

#include "app.h"
#include "app_manifest.h"
#include "app_types.h"
#include "ardubot_keys.h"
#include "icons.h"
#include "power.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#define ARDUBOT_CONSTRUCTOR(fn)                                                \
  __pragma(section(".CRT$XCU", read))                                          \
      __declspec(allocate(".CRT$XCU")) static void (*fn##_ctor)(void) = (fn);
#else
#define ARDUBOT_CONSTRUCTOR(fn) __attribute__((constructor))
#endif

#define APP_KIT_MAX_KEYS 8

typedef void (*app_fn_t)(void *app);
typedef void (*app_key_fn_t)(void *app, void *user);

typedef struct {
  const char *name;
  const char *version;
  const char *author;
  const char *description;
  app_type_t type;
  uint32_t fps;
  uint32_t stack_size;
  uint32_t heap_size;
  const app_icon_t *icon;
  app_fn_t on_init;
  app_fn_t on_frame;
  app_fn_t on_cleanup;
} app_desc_t;

typedef struct {
  sim_key_t key;
  int pin;
  app_key_fn_t fn;
  void *user;
  void *app;
} app_key_binding_t;

#ifndef APP_CTX_T_DECLARED
#define APP_CTX_T_DECLARED
typedef struct app_ctx app_ctx_t;
#endif

struct app_ctx {
  app_display_t display;
  app_timer_t timer;
  bool dirty;
  bool running;
  const app_desc_t *desc;
  void *user;
  app_key_binding_t keys[APP_KIT_MAX_KEYS];
  uint32_t key_count;
  app_ui_config_t ui;
  power_demand_t demand;
  bool demand_set;
  task_tcb_t *demand_task;
  /* .paced keeps the frame clock (.live / .game). Otherwise the loop
   * sleeps until .every_ms, a notification, or a key. */
  bool paced;
  uint32_t every_ms;
  task_tcb_t *run_task;
};

int app_bind_key(app_ctx_t *app, sim_key_t key, app_key_fn_t fn, void *user);

int app_bind_back(app_ctx_t *app);

int app_open(app_ctx_t *from, const char *name);

/** Name of the focused app, or NULL when nothing is focused. */
const char *app_kit_foreground_name(void);

/**
 * Resume or start `name`, focus it, and suspend the app that was in front.
 * The caller does not have to be an app.
 */
int app_kit_switch(const char *name);

void app_request_exit(app_ctx_t *app);

/* app_request_exit() takes one argument; app_key_fn_t (a key handler)
 * takes two (app, user). Every stdapp that binds Escape straight to
 * app_request_exit() used to do `(app_key_fn_t)app_request_exit` -
 * casting a 1-arg function pointer to a 2-arg function pointer type is
 * flagged by gcc 13's -Wcast-function-type under -Werror (arity
 * mismatch, not just an unrelated-pointee-type cast). This trampoline
 * already has app_key_fn_t's shape; bind it directly instead:
 * `(app_key_fn_t)app_request_exit_key` (same cast style app_bind_back()
 * already uses internally for the identical pattern - only the
 * app_ctx_t*-vs-void* first parameter differs, which this compiler/flag
 * combination doesn't flag). */
void app_request_exit_key(app_ctx_t *app, void *user);

bool app_kit_is_foreground(const app_ctx_t *app);
bool app_kit_is_foreground_desc(const app_desc_t *desc);

/* Publish how much compute this app wants. The governor treats it as a
 * ceiling. Leaving the foreground drops the live hint back to NORMAL;
 * the stored demand is applied again when the app is focused. */
int app_kit_set_demand(app_ctx_t *app, power_demand_t demand);
power_demand_t app_kit_get_demand(const app_ctx_t *app);

void app_kit_run(const app_desc_t *desc);

/* How long the run loop should sleep before the next frame. A tool with
 * no .every_ms waits instead of polling at .fps. */
uint32_t app_kit_next_sleep_ms(const app_ctx_t *app);

/* Pull a blocked app's sleep forward so a key or a notification draws
 * on the next scheduler tick. */
void app_kit_wake(app_ctx_t *app);
void app_kit_wake_foreground(void);

app_manifest_t *app_kit_make_manifest(const app_desc_t *desc,
                                      void (*entry)(void));

void app_kit_apply_overrides(app_desc_t *dest, const app_desc_t *over);

void app_kit_set_icon(const char *name, const app_icon_t *icon);
const app_icon_t *app_kit_get_icon(const char *name);

#include "canvas.h"
#include "catalog.h"
#include "menu.h"
#include "screen.h"
#include "status.h"

#define APP_DEFINE(symbol, install_name, ...)                                  \
  static app_desc_t symbol##_desc;                                             \
  static void symbol##_entry(void);                                            \
  app_manifest_t *symbol##_manifest = NULL;                                    \
  static void symbol##_entry(void) { app_kit_run(&symbol##_desc); }            \
  static void symbol##_register(void);                                         \
  ARDUBOT_CONSTRUCTOR(symbol##_register)                                       \
  static void symbol##_register(void) {                                        \
    symbol##_desc = (app_desc_t){                                              \
        .name = (install_name),                                                \
        .version = app_manifest_version(install_name),                         \
        .author = app_manifest_author(install_name),                           \
        .description = app_manifest_description(install_name),                 \
        .type = APP_TYPE_USER,                                                 \
        .fps = 30,                                                             \
        .stack_size = APP_STACK_SMALL,                                         \
        .heap_size = APP_HEAP_SMALL,                                           \
        .icon = NULL,                                                          \
    };                                                                         \
    {                                                                          \
      const app_desc_t _app_over = {__VA_ARGS__};                              \
      app_kit_apply_overrides(&symbol##_desc, &_app_over);                     \
    }                                                                          \
    if (symbol##_desc.icon) {                                                  \
      app_kit_set_icon(install_name, symbol##_desc.icon);                      \
    }                                                                          \
    symbol##_manifest = app_kit_make_manifest(&symbol##_desc, symbol##_entry); \
  }

extern int g_next_pin;

#ifdef __cplusplus
}
#endif
