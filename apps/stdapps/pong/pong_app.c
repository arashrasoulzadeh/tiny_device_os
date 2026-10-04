#include "app_framework.h"
#include "pong.h"
#include "sim_gpio.h"

extern const app_icon_t pong_app_icon;

static pong_t g_pong;
static app_ui_t g_ui;

static void fill_rect(app_ui_t* app, int x, int y, int w, int h) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            app_ui_pixel(app, x + dx, y + dy, true);
        }
    }
}

static void on_init(void* app) {
    (void)app;
    app_ui_config_t cfg;
    app_ui_config_game(&cfg);
    app_ui_init(&g_ui, app, &cfg);
    pong_reset(&g_pong);

    /* on_frame polls these pins directly (continuous "held" state, unlike
     * app_ui_bind_key's press/release callbacks) - register and map them
     * ourselves since nothing else wires pins 0-2 to real keys. */
    sim_gpio_register(0, false);
    sim_gpio_register(1, false);
    sim_gpio_register(2, false);
    sim_gpio_set_key_mapping(SIM_KEY_UP, 0, true);
    sim_gpio_set_key_mapping(SIM_KEY_DOWN, 1, true);
    sim_gpio_set_key_mapping(SIM_KEY_ENTER, 2, true);

    app_ui_bind_key(&g_ui, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL);

    APP_INFO("Pong ready - Up/Down/Enter to move paddle");
}

static void on_frame(void* app) {
    (void)app;
    if (!g_pong.game_over) {
        if (sim_gpio_read(0)) {  // UP
            pong_paddle_up(&g_pong);
        }
        if (sim_gpio_read(1) || sim_gpio_read(2)) {  // DOWN or ENTER
            pong_paddle_down(&g_pong);
        }
        pong_step(&g_pong);
    }
    if (!app_is_dirty(app)) {
        return;
    }
    app_ui_begin_frame(&g_ui);
    
    int wall_x = g_ui.ui.content_w - 2;
    for (int y = 0; y < g_ui.ui.content_h; y++) {
        app_ui_pixel(&g_ui, wall_x, y, true);
        app_ui_pixel(&g_ui, wall_x + 1, y, true);
    }
    
    fill_rect(&g_ui, PONG_PADDLE_X, g_pong.paddle_y, PONG_PADDLE_W, PONG_PADDLE_H);
    fill_rect(&g_ui, g_pong.ball_x, g_pong.ball_y, PONG_BALL, PONG_BALL);
    
    if (g_pong.game_over) {
        app_ui_textf(&g_ui, 46, 4, "END");
        app_ui_textf(&g_ui, 28, 16, "S:%u", g_pong.score);
    } else {
        app_ui_textf(&g_ui, 0, 0, "%u", g_pong.score);
    }
    app_ui_end_frame(&g_ui);
}

static void on_cleanup(void* app) {
    (void)app;
    app_ui_deinit(&g_ui);
}

APP_DEFINE(pong_app, "pong", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Pong - Up/Down paddle, Select restarts", .type = APP_TYPE_GAME,
           .icon = &pong_app_icon, .fps = 30, .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
