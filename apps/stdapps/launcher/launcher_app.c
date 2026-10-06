#include "app_framework.h"

static app_menu_t g_menu;

static void on_launch(void* app, void* user) {
    const app_menu_item_t* item;
    (void)user;
    item = app_menu_selected(&g_menu);
    if (item) {
        app_open((app_ctx_t*)app, item->id);
    }
}

static void on_ready(app_helper_t* app) {
    int keep = g_menu.selected;
    app_kit_catalog_build("launcher");
    if (APP_DISPLAY_HEIGHT <= 32) {
        app_menu_init(&g_menu, 8, 10);
        app_menu_load_catalog(&g_menu);
        APP_INFO("Launcher list ready (%d apps)", app_kit_catalog_count());
    } else {
        app_menu_init(&g_menu, 8, 8);
        app_menu_set_icon_strip(&g_menu);
        app_menu_load_catalog(&g_menu);
        APP_INFO("Launcher icon strip ready (%d apps)", app_kit_catalog_count());
    }
    if (app_helper_has_state(app) && keep >= 0 && keep < g_menu.count) {
        g_menu.selected = keep;
    }
    APP_INFO("Launcher ready");
}

static void on_draw(app_helper_t* app) {
    app_menu_draw(&app->ui.ctx, &g_menu, NULL, NULL);
}

static void on_cleanup(app_helper_t* app) {
    (void)app;
    app_kit_catalog_clear();
    app_menu_clear(&g_menu);
    APP_INFO("Launcher cleaned up");
}

/* Up/Left move to the previous item. Down/Right move to the next. */
static const app_ui_key_def_t launcher_keys[] = {
    {SIM_KEY_UP, app_ui_menu_nav_prev, &g_menu},
    {SIM_KEY_DOWN, app_ui_menu_nav_next, &g_menu},
    {SIM_KEY_RIGHT, app_ui_menu_nav_next, &g_menu},
    {SIM_KEY_LEFT, app_ui_menu_nav_prev, &g_menu},
    {SIM_KEY_ENTER, on_launch, NULL},
    {SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL},
    {0, NULL, NULL},
};

APP_HELPER(launcher_app, "launcher", .type = APP_TYPE_SYSTEM, .live = true,
           .state = &g_menu.selected, .state_size = sizeof(g_menu.selected),
           .keys = launcher_keys, .on_ready = on_ready, .on_draw = on_draw,
           .on_cleanup = on_cleanup)
