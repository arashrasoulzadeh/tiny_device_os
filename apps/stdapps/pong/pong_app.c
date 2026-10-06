#include "app_framework.h"
#include "app_helper.h"
#include "pong.h"
#include "sim_gpio.h"

extern const app_icon_t pong_app_icon;

static pong_t g_pong;

static void fill_rect_color(app_ui_t* app, int x, int y, int w, int h, uint16_t rgb565) {
    app_ui_rect_color(app, x, y, w, h, 0, rgb565);
}

static void on_ready(app_helper_t* app) {
    if (!app_helper_has_state(app)) {
        pong_reset(&g_pong);
    }

    /* on_tick polls these pins directly (continuous "held" state, unlike
     * a key callback's press edge). Register them before Escape so the
     * exit binding does not take pin 0. */
    sim_gpio_register(0, false);
    sim_gpio_register(1, false);
    sim_gpio_register(2, false);
    sim_gpio_set_key_mapping(SIM_KEY_UP, 0, true);
    sim_gpio_set_key_mapping(SIM_KEY_DOWN, 1, true);
    sim_gpio_set_key_mapping(SIM_KEY_ENTER, 2, true);
    app_ui_bind_key(&app->ui, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL);

    APP_INFO("Pong ready - Up/Down/Enter to move paddle");
}

static void on_tick(app_helper_t* app) {
    (void)app;
    if (g_pong.game_over) {
        return;
    }
    if (sim_gpio_read(0)) {
        pong_paddle_up(&g_pong);
    }
    if (sim_gpio_read(1) || sim_gpio_read(2)) {
        pong_paddle_down(&g_pong);
    }
    pong_step(&g_pong);
}

static void on_draw(app_helper_t* app) {
    fill_rect_color(&app->ui, app->ui.ui.content_w - 2, 0, 2, app->ui.ui.content_h,
                    APP_UI_RGB565(140, 140, 140));
    fill_rect_color(&app->ui, PONG_PADDLE_X, g_pong.paddle_y, PONG_PADDLE_W, PONG_PADDLE_H,
                    APP_UI_RGB565(0, 252, 248));
    fill_rect_color(&app->ui, g_pong.ball_x, g_pong.ball_y, PONG_BALL, PONG_BALL,
                    APP_UI_RGB565(248, 252, 0));

    if (g_pong.game_over) {
        app_ui_textf(&app->ui, 46, 4, "END");
        app_ui_textf(&app->ui, 28, 16, "S:%u", g_pong.score);
    } else {
        app_ui_textf(&app->ui, 0, 0, "%u", g_pong.score);
    }
}

/* Empty table skips the default Up/Down/Select bindings. Those pins are
 * polled as held buttons in on_tick; Escape is bound after that wiring. */
static const app_ui_key_def_t pong_keys[] = {
    {0, NULL, NULL},
};

APP_HELPER(pong_app, "pong", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Pong - Up/Down paddle, Select restarts", .type = APP_TYPE_GAME,
           .icon = &pong_app_icon, .fps = 30, .game = true, .live = true, .keys = pong_keys,
           .state = &g_pong, .state_size = sizeof(g_pong),
           .on_ready = on_ready, .on_tick = on_tick, .on_draw = on_draw)
