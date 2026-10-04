#include "app_framework.h"
#include "app_kit.h"
#include "catalog.h"
#include "menu.h"
#include "launcher_icon.h"

static app_menu_t g_menu;
static app_ui_t g_ui;

static void on_launch(void* app, void* user) {
    (void)user;
    const app_menu_item_t* item = app_menu_selected(&g_menu);
    if (item) {
        app_open((app_ctx_t*)app, item->id);
    }
}

static void on_init(void* app) {
    (void)app;
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "LAUNCHER", "Up/Dn:Nav Sel:Launch Bk:Back");
    app_ui_init(&g_ui, app, &cfg);

    app_kit_catalog_build("launcher");
    /* Use icon strip on larger displays, list layout on small (32px) displays */
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

    /* Up moves to the previous item, Down to the next - Left/Right below
     * already get this right (Right=next, Left=prev); Up/Down had next and
     * prev swapped, so Down moved the selection backward and vice versa. */
    app_ui_bind_key(&g_ui, SIM_KEY_UP, app_ui_menu_nav_prev, &g_menu);
    app_ui_bind_key(&g_ui, SIM_KEY_DOWN, app_ui_menu_nav_next, &g_menu);
    app_ui_bind_key(&g_ui, SIM_KEY_RIGHT, app_ui_menu_nav_next, &g_menu);
    app_ui_bind_key(&g_ui, SIM_KEY_LEFT, app_ui_menu_nav_prev, &g_menu);
    app_ui_bind_key(&g_ui, SIM_KEY_ENTER, on_launch, NULL);
    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL);

    APP_INFO("Launcher ready");
}

static void on_frame(void* app) {
    app_mark_dirty((app_ctx_t*)app);
    app_ui_begin_frame(&g_ui);
    app_menu_draw((app_ctx_t*)app, &g_menu, NULL, NULL);
    app_ui_end_frame(&g_ui);
    app_clear_dirty((app_ctx_t*)app);
}

static void on_cleanup(void* app) {
    (void)app;
    app_kit_catalog_clear();
    app_menu_clear(&g_menu);
    app_ui_deinit(&g_ui);
    APP_INFO("Launcher cleaned up");
}

APP_DEFINE(launcher_app, "launcher",
    .version = "1.0.0",
    .author = "ArdubotOS",
    .description = "System launcher / home app",
    .type = APP_TYPE_SYSTEM,
    .fps = 30,
    .icon = &launcher_app_icon,
    .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup
)