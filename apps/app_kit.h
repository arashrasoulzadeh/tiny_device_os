#pragma once

#include "app_framework.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_KIT_MAX_KEYS 8

struct app_ctx;

typedef void (*app_fn_t)(struct app_ctx* app);
typedef void (*app_key_fn_t)(struct app_ctx* app, void* user);

typedef struct {
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    app_type_t type;
    uint32_t fps;
    uint32_t stack_size;
    uint32_t heap_size;
    app_fn_t on_init;
    app_fn_t on_frame;
    app_fn_t on_cleanup;
} app_desc_t;

typedef struct {
    sim_key_t key;
    int pin;
    app_key_fn_t fn;
    void* user;
    struct app_ctx* app;
} app_key_binding_t;

typedef struct app_ctx {
    app_display_t display;
    app_timer_t timer;
    bool dirty;
    bool running;
    const app_desc_t* desc;
    void* user;
    app_key_binding_t keys[APP_KIT_MAX_KEYS];
    uint32_t key_count;
} app_ctx_t;

void app_mark_dirty(app_ctx_t* app);
void app_clear_dirty(app_ctx_t* app);
bool app_is_dirty(const app_ctx_t* app);

void app_clear(app_ctx_t* app);
void app_text(app_ctx_t* app, int x, int y, const char* text);
void app_textf(app_ctx_t* app, int x, int y, const char* fmt, ...);
void app_flush(app_ctx_t* app);

int app_bind_key(app_ctx_t* app, sim_key_t key, app_key_fn_t fn, void* user);

/** Launch another installed app; suspends the caller and focuses the new app. */
int app_open(app_ctx_t* from, const char* name);

/** Leave the current app (Escape/back). Resumes the home/launcher on exit. */
void app_request_exit(app_ctx_t* app);

bool app_kit_is_foreground(const app_ctx_t* app);

/* --- Launch catalog (built once at OS boot after installs) --- */

#define APP_KIT_CATALOG_MAX 16

typedef struct {
    const char* name;
    app_type_t type;
} app_catalog_entry_t;

/** Snapshot launchable apps (skips `exclude_name`, typically "launcher"). */
int app_kit_catalog_build(const char* exclude_name);
void app_kit_catalog_clear(void);
int app_kit_catalog_count(void);
const app_catalog_entry_t* app_kit_catalog_at(int index);

/** Short tag for menu columns, e.g. "[TOL]". */
const char* app_type_tag(app_type_t type);

/* --- Simple vertical menu --- */

#define APP_KIT_MENU_MAX APP_KIT_CATALOG_MAX

typedef struct {
    const char* id;
    const char* label;
    const char* tag;
} app_menu_item_t;

typedef struct {
    app_menu_item_t items[APP_KIT_MENU_MAX];
    int count;
    int selected;
    int first_visible;
    int start_y;
    int row_h;
} app_menu_t;

void app_menu_init(app_menu_t* menu, int start_y, int row_h);
void app_menu_clear(app_menu_t* menu);
int app_menu_add(app_menu_t* menu, const char* id, const char* label, const char* tag);
/** Fill menu from the boot catalog (call once). */
int app_menu_load_catalog(app_menu_t* menu);
/** Move selection by delta; scrolls into view. Returns true if selection changed. */
bool app_menu_move(app_menu_t* menu, int delta, int display_h);
const app_menu_item_t* app_menu_selected(const app_menu_t* menu);
void app_menu_draw(app_ctx_t* app, app_menu_t* menu, int display_h, const char* title,
                   const char* help);

void app_kit_run(const app_desc_t* desc);

app_manifest_t* app_kit_make_manifest(const app_desc_t* desc, void (*entry)(void));

void app_kit_apply_overrides(app_desc_t* dest, const app_desc_t* over);

/**
 * Declare a builtin app.
 *
 * @param symbol       C prefix (`<symbol>_manifest` exported for the OS/sim)
 * @param install_name Runtime name passed to app_install/app_start
 *
 * Example:
 *   APP_DEFINE(counter_app, "counter",
 *       .version = "2.0.0",
 *       .on_init = on_init,
 *       .on_frame = on_frame
 *   );
 */
#define APP_DEFINE(symbol, install_name, ...)                                              \
    static app_desc_t symbol##_desc;                                                       \
    static void symbol##_entry(void);                                                      \
    app_manifest_t* symbol##_manifest = NULL;                                              \
    static void symbol##_entry(void) {                                                     \
        app_kit_run(&symbol##_desc);                                                       \
    }                                                                                      \
    __attribute__((constructor)) static void symbol##_register(void) {                     \
        symbol##_desc = (app_desc_t){                                                      \
            .name = (install_name),                                                        \
            .version = "1.0.0",                                                            \
            .author = "ArdubotOS",                                                         \
            .description = (install_name),                                                 \
            .type = APP_TYPE_USER,                                                         \
            .fps = 30,                                                                     \
            .stack_size = APP_STACK_SMALL,                                                 \
            .heap_size = APP_HEAP_SMALL,                                                   \
        };                                                                                 \
        {                                                                                  \
            const app_desc_t _app_over = {__VA_ARGS__};                                    \
            app_kit_apply_overrides(&symbol##_desc, &_app_over);                           \
        }                                                                                  \
        symbol##_manifest = app_kit_make_manifest(&symbol##_desc, symbol##_entry);         \
    }

#ifdef __cplusplus
}
#endif
