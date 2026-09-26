#include "app_kit.h"

static app_menu_t g_menu;
static bool g_menu_ready;

static void on_select(app_ctx_t* app, void* user) {
    const app_menu_item_t* item = app_menu_selected(&g_menu);
    (void)user;
    if (!item || !item->id) {
        return;
    }
    APP_INFO("Launching %s", item->id);
    if (app_open(app, item->id) != 0) {
        APP_ERROR("Failed to open %s", item->id);
    }
}

static void on_init(app_ctx_t* app) {
    app_menu_bind_nav(app, &g_menu);
    app_bind_key(app, SIM_KEY_ENTER, on_select, NULL);

    /* Catalog is built once at OS boot; load into the menu a single time. */
    if (!g_menu_ready) {
        app_menu_init(&g_menu, 16, 12);
        app_menu_load_catalog(&g_menu);
        g_menu_ready = true;
    }

    app_mark_dirty(app);
    APP_INFO("Launcher ready (%d apps)", g_menu.count);
}

static void on_frame(app_ctx_t* app) {
    if (!app_is_dirty(app)) {
        return;
    }
    app_menu_draw(app, &g_menu, "ArdubotOS Launcher", "Up/Dn Enter");
}

APP_DEFINE(launcher_app, "launcher", .version = "1.0.0", .author = "ArdubotOS",
           .description = "System launcher / home app", .type = APP_TYPE_SYSTEM, .fps = 30,
           .on_init = on_init, .on_frame = on_frame);
