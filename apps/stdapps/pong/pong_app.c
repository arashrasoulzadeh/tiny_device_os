#include "app_kit.h"
#include "pong.h"

extern const app_icon_t pong_app_icon;

static pong_t g_pong;

static bool key_held(const app_ctx_t* app, sim_key_t key) {
    uint32_t i;
    if (!app) {
        return false;
    }
    for (i = 0; i < app->key_count; i++) {
        if (app->keys[i].key == key && sim_gpio_read(app->keys[i].pin)) {
            return true;
        }
    }
    return false;
}

static void on_select(app_ctx_t* app, void* user) {
    (void)user;
    if (!g_pong.game_over) {
        return;
    }
    pong_reset(&g_pong);
    app_mark_dirty(app);
}

static void fill_rect(app_ctx_t* app, int x, int y, int w, int h) {
    int dy;
    int dx;
    for (dy = 0; dy < h; dy++) {
        for (dx = 0; dx < w; dx++) {
            app_pixel(app, x + dx, y + dy, true);
        }
    }
}

static void on_init(app_ctx_t* app) {
    app_bind_key(app, SIM_KEY_UP, on_select, NULL);
    app_bind_key(app, SIM_KEY_DOWN, on_select, NULL);
    app_bind_key(app, SIM_KEY_ENTER, on_select, NULL);
    app_bind_back(app);
    pong_reset(&g_pong);
    APP_INFO("Pong ready — Up/Down paddle, Sel restarts, hold back");
}

static void on_frame(app_ctx_t* app) {
    int y;
    if (!g_pong.game_over) {
        if (key_held(app, SIM_KEY_UP)) {
            pong_paddle_up(&g_pong);
        }
        if (key_held(app, SIM_KEY_DOWN) || key_held(app, SIM_KEY_ENTER)) {
            pong_paddle_down(&g_pong);
        }
        pong_step(&g_pong);
        app_mark_dirty(app);
    }
    if (!app_is_dirty(app)) {
        return;
    }
    app_clear(app);
    for (y = 0; y < PONG_H; y++) {
        app_pixel(app, PONG_W - 2, y, true);
        app_pixel(app, PONG_W - 1, y, true);
    }
    fill_rect(app, PONG_PADDLE_X, g_pong.paddle_y, PONG_PADDLE_W, PONG_PADDLE_H);
    fill_rect(app, g_pong.ball_x, g_pong.ball_y, PONG_BALL, PONG_BALL);
    if (g_pong.game_over) {
        app_text(app, 46, 4, "END");
        app_textf(app, 28, 16, "S:%u Sel", g_pong.score);
    } else {
        app_textf(app, 0, 0, "%u", g_pong.score);
    }
    app_status_draw(app);
    app_flush(app);
    app_clear_dirty(app);
}

APP_DEFINE(pong_app, "pong", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Pong — Up/Down paddle, Select restarts", .type = APP_TYPE_GAME,
           .icon = &pong_app_icon, .fps = 30, .on_init = on_init, .on_frame = on_frame)
