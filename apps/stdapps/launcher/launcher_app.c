#include "app_kit.h"
#include <stdint.h>

/* Unlimited horizontal icon strip — matches NodeMCU board launcher. */

static app_menu_t g_menu;
static app_timer_t g_timer;

static void on_launch(app_ctx_t* app, void* user) {
    (void)user;
    const app_menu_item_t* item = app_menu_selected(&g_menu);
    if (item) {
        app_open(app, item->id);
    }
}

static void launcher_init(app_ctx_t* app) {
    if (app_display_init(&app->display, "/dev/display0") != 0) {
        APP_ERROR("Display init failed");
        return;
    }

    app_timer_init(&g_timer, 30);

    app_kit_catalog_build("launcher");
    app_menu_init(&g_menu, 8, 8);
    app_menu_set_icon_strip(&g_menu);
    app_menu_load_catalog(&g_menu);

    app_menu_bind_nav(app, &g_menu);
    app_bind_key(app, SIM_KEY_ENTER, on_launch, NULL);
    app_bind_back(app);

    APP_INFO("Launcher icon strip ready (%d apps)", app_kit_catalog_count());
}

static void launcher_frame(app_ctx_t* app) {
    if (!app_is_dirty(app)) {
        if (app_timer_should_frame(&g_timer)) {
            app_timer_sleep_remaining(&g_timer);
        }
        return;
    }

    app_menu_draw(app, &g_menu, NULL, NULL);
    app_clear_dirty(app);

    if (app_timer_should_frame(&g_timer)) {
        app_timer_sleep_remaining(&g_timer);
    }
}

static void launcher_cleanup(app_ctx_t* app) {
    app_display_deinit(&app->display);
    app_kit_catalog_clear();
    app_menu_clear(&g_menu);
    APP_INFO("Launcher cleaned up");
}

APP_DEFINE(launcher_app, "launcher",
    .version = "1.0.0",
    .author = "ArdubotOS",
    .description = "System launcher / home app",
    .type = APP_TYPE_SYSTEM,
    .fps = 30,
    .on_init = launcher_init,
    .on_frame = launcher_frame,
    .on_cleanup = launcher_cleanup
)
