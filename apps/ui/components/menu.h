#pragma once

#include "canvas.h"
#include "catalog.h"
#include "display.h"
#include "icons.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_KIT_MENU_MAX APP_KIT_CATALOG_MAX

#define APP_MENU_ONE_UP (-1)
#define APP_MENU_ONE_DOWN (1)

typedef enum {
    APP_MENU_LAYOUT_LIST = 0,
    APP_MENU_LAYOUT_ICONS, /* unlimited horizontal icon strip */
} app_menu_layout_t;

typedef struct {
    const char* id;
    const char* label;
    const char* tag;
    const app_icon_t* icon;
} app_menu_item_t;

typedef struct {
    app_menu_item_t items[APP_KIT_MENU_MAX];
    int count;
    int selected;
    int first_visible; /* list mode only */
    int start_y;
    int row_h;
    app_menu_layout_t layout;
} app_menu_t;

void app_menu_init(app_menu_t* menu, int start_y, int row_h);
/** Horizontal infinite icon carousel (launcher). */
void app_menu_set_icon_strip(app_menu_t* menu);
void app_menu_clear(app_menu_t* menu);
int app_menu_add(app_menu_t* menu, const char* id, const char* label, const char* tag,
                 const app_icon_t* icon);
int app_menu_load_catalog(app_menu_t* menu);

bool app_menu_move(app_ctx_t* app, app_menu_t* menu, int delta, void* user);

const app_menu_item_t* app_menu_selected(const app_menu_t* menu);
void app_menu_draw(app_ctx_t* app, app_menu_t* menu, const char* title, const char* help);

/** Bind Up/Down/Left/Right for navigation. */
int app_menu_bind_nav(app_ctx_t* app, app_menu_t* menu);

#ifdef __cplusplus
}
#endif
