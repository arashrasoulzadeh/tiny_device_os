#include "app_framework.h"
#include "fw/ui.h"
#include "header_app.h"
#include "scheduler.h"
#include "sim_i2c.h"
#include "sim_video.h"
#include "unity.h"

void setUp(void) {
    scheduler_init();
    header_app_set_visible(false);
}

void tearDown(void) {
    header_app_set_visible(false);
}

static uint32_t pixel_at(int x, int y) {
    uint32_t* pixels = sim_video_get_pixels();
    int sim_w = sim_video_get_width();
    if (!pixels || x < 0 || y < 0 || x >= sim_w) {
        return 0;
    }
    return pixels[y * sim_w + x];
}

static bool video_ready(void) {
    sim_i2c_init();
    if (sim_video_init(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT, "header-app") != 0 ||
        !sim_video_get_pixels()) {
        sim_i2c_cleanup();
        return false;
    }
    return true;
}

static void video_done(void) {
    sim_video_cleanup();
    sim_i2c_cleanup();
}

static void cover_the_band(app_display_t* disp) {
    app_display_fill_rect_color(disp, 0, 0, APP_DISPLAY_WIDTH, APP_HEADER_BAND, 0,
                                APP_UI_RGB565(255, 0, 0));
}

/* Flush is the only paint path. A caller that never calls app_header_draw
 * still gets the band when the screen asked for it. */
void test_flush_reads_the_band_from_header_app(void) {
    app_ui_config_t cfg;
    app_ui_t ui;

    if (app_header_height() <= 0) {
        TEST_IGNORE_MESSAGE("panel is too small for the header band");
        return;
    }
    if (!video_ready()) {
        TEST_IGNORE_MESSAGE("SDL video unavailable (headless / no display)");
        return;
    }

    app_ui_config_ui(&cfg, "TEST", "help");
    TEST_ASSERT_EQUAL(0, app_ui_init(&ui, NULL, &cfg));
    header_app_set_visible(true);
    cover_the_band(&ui.ctx->display);
    app_display_flush(&ui.ctx->display);
    TEST_ASSERT_EQUAL_UINT32(0xFF000000u, pixel_at(2, 2));
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, pixel_at(19, 5));
    app_ui_deinit(&ui);
    video_done();
}

static uint32_t g_band_pixel;

static void on_draw_cover_and_flush(app_helper_t* app) {
    cover_the_band(&app->ui.ctx->display);
    /* The launcher menu flushes from its own draw, before end_frame. */
    app_display_flush(&app->ui.ctx->display);
    g_band_pixel = pixel_at(19, 5);
}

static void run_cover_flush(const app_helper_desc_t* desc) {
    app_helper_t app;
    g_band_pixel = 0;
    TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, desc));
    app_helper_frame(&app);
    app_helper_stop(&app);
}

void test_standard_apps_including_the_launcher_show_header_app(void) {
    app_helper_desc_t desc = {
        .name = "launcher",
        .title = "LAUNCHER",
        .on_draw = on_draw_cover_and_flush,
    };

    if (app_header_height() <= 0) {
        TEST_IGNORE_MESSAGE("panel is too small for the header band");
        return;
    }
    if (!video_ready()) {
        TEST_IGNORE_MESSAGE("SDL video unavailable (headless / no display)");
        return;
    }

    run_cover_flush(&desc);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, g_band_pixel);
    video_done();
}

void test_game_and_fullscreen_do_not_show_header_app(void) {
    app_helper_desc_t game = {
        .title = "GAME",
        .game = true,
        .on_draw = on_draw_cover_and_flush,
    };
    app_helper_desc_t fullscreen = {
        .title = "FS",
        .fullscreen = true,
        .on_draw = on_draw_cover_and_flush,
    };

    if (app_header_height() <= 0) {
        TEST_IGNORE_MESSAGE("panel is too small for the header band");
        return;
    }
    if (!video_ready()) {
        TEST_IGNORE_MESSAGE("SDL video unavailable (headless / no display)");
        return;
    }

    run_cover_flush(&game);
    TEST_ASSERT_EQUAL_UINT32(0xFFFF0000u, g_band_pixel);
    run_cover_flush(&fullscreen);
    TEST_ASSERT_EQUAL_UINT32(0xFFFF0000u, g_band_pixel);
    video_done();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flush_reads_the_band_from_header_app);
    RUN_TEST(test_standard_apps_including_the_launcher_show_header_app);
    RUN_TEST(test_game_and_fullscreen_do_not_show_header_app);
    return UNITY_END();
}
