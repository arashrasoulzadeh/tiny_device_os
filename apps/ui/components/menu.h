#pragma once

#include "canvas.h"
#include "catalog.h"
#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_KIT_MENU_MAX APP_KIT_CATALOG_MAX

#define APP_MENU_ONE_UP (-1)
#define APP_MENU_ONE_DOWN (1)

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

/**
 * Move selection by @p delta (e.g. APP_MENU_ONE_UP / APP_MENU_ONE_DOWN).
 * Consumes @p user (for key handlers), scrolls using APP_DISPLAY_HEIGHT, and
 * marks @p app dirty when the selection changes.
 */
bool app_menu_move(app_ctx_t* app, app_menu_t* menu, int delta, void* user);

const app_menu_item_t* app_menu_selected(const app_menu_t* menu);
void app_menu_draw(app_ctx_t* app, app_menu_t* menu, const char* title, const char* help);

/** Bind Up/Down to move @p menu (stores menu pointer as key user). */
int app_menu_bind_nav(app_ctx_t* app, app_menu_t* menu);

#ifdef __cplusplus
}
#endif
