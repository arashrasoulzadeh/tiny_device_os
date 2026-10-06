#include "unity.h"
#include "app_helper.h"
#include "scheduler.h"
#include "theme.h"

#include <string.h>

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_fmt_clock_mmss(void) {
    char buf[8];
    TEST_ASSERT_EQUAL(5, app_fmt_clock(buf, sizeof(buf), 125));
    TEST_ASSERT_EQUAL_STRING("02:05", buf);
}

void test_fmt_clock_clamps_negative_and_allows_long_minutes(void) {
    char buf[16];
    TEST_ASSERT_EQUAL(5, app_fmt_clock(buf, sizeof(buf), -3));
    TEST_ASSERT_EQUAL_STRING("00:00", buf);
    TEST_ASSERT_GREATER_THAN(5, app_fmt_clock(buf, sizeof(buf), 100 * 60));
    TEST_ASSERT_EQUAL_STRING("100:00", buf);
}

void test_fmt_clock_rejects_a_buffer_that_cannot_hold_the_text(void) {
    char buf[4];
    memset(buf, 'x', sizeof(buf));
    TEST_ASSERT_EQUAL(-1, app_fmt_clock(buf, sizeof(buf), 125));
    TEST_ASSERT_EQUAL(-1, app_fmt_clock(NULL, 8, 1));
    TEST_ASSERT_EQUAL(-1, app_fmt_clock(buf, 0, 1));
}

void test_fit_text_scale_clamps_to_the_given_max_and_at_least_one(void) {
    TEST_ASSERT_EQUAL(6, app_fit_text_scale(320, 2, 6));
    TEST_ASSERT_EQUAL(1, app_fit_text_scale(10, 5, 6));
    TEST_ASSERT_EQUAL(1, app_fit_text_scale(0, 2, 6));
    TEST_ASSERT_EQUAL(1, app_fit_text_scale(100, 0, 4));
    TEST_ASSERT_EQUAL(1, app_fit_text_scale(100, 2, 0));
}

void test_center_in_places_the_item_inside_the_box(void) {
    TEST_ASSERT_EQUAL(50, app_center_in(10, 100, 20));
}

void test_bar_fill_maps_and_clamps(void) {
    TEST_ASSERT_EQUAL(100, app_bar_fill_px(50, 100, 200));
    TEST_ASSERT_EQUAL(0, app_bar_fill_px(0, 100, 200));
    TEST_ASSERT_EQUAL(0, app_bar_fill_px(-4, 100, 200));
    TEST_ASSERT_EQUAL(200, app_bar_fill_px(150, 100, 200));
    TEST_ASSERT_EQUAL(0, app_bar_fill_px(10, 0, 200));
}

void test_gauge_fill_is_signed_and_clamped_to_the_range(void) {
    TEST_ASSERT_EQUAL(20, app_gauge_fill_px(10, 20, 40));
    TEST_ASSERT_EQUAL(-20, app_gauge_fill_px(-10, 20, 40));
    TEST_ASSERT_EQUAL(40, app_gauge_fill_px(100, 20, 40));
    TEST_ASSERT_EQUAL(0, app_gauge_fill_px(0, 20, 40));
    TEST_ASSERT_EQUAL(0, app_gauge_fill_px(5, 0, 40));
}

void test_level_color_uses_the_shared_palette(void) {
    TEST_ASSERT_EQUAL_UINT16(ARDUBOT_COLOR_DANGER, app_level_color(5, 30, 10));
    TEST_ASSERT_EQUAL_UINT16(ARDUBOT_COLOR_WARNING, app_level_color(20, 30, 10));
    TEST_ASSERT_EQUAL_UINT16(ARDUBOT_COLOR_TEXT, app_level_color(40, 30, 10));
}

static int g_events;
static app_helper_event_t g_last_event;
static int g_draws;

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    g_events++;
    g_last_event = ev;
}

static void on_draw(app_helper_t* app) {
    (void)app;
    g_draws++;
}

void test_start_binds_the_standard_keys_and_draws_once_until_invalidated(void) {
    app_helper_desc_t desc = {
        .title = "TEST",
        .help = "Up / Sel",
        .on_event = on_event,
        .on_draw = on_draw,
    };
    app_helper_t app;
    g_events = 0;
    g_draws = 0;

    TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
    TEST_ASSERT_EQUAL(8, app.ui.ctx.key_count);
    TEST_ASSERT_EQUAL(SIM_KEY_DOWN, app.ui.ctx.keys[2].key);
    TEST_ASSERT_EQUAL(SIM_KEY_LEFT, app.ui.ctx.keys[3].key);
    TEST_ASSERT_EQUAL(SIM_KEY_RIGHT, app.ui.ctx.keys[4].key);

    app_helper_frame(&app);
    app_helper_frame(&app);
    TEST_ASSERT_EQUAL(1, g_draws);

    app_helper_emit(&app, APP_EV_UP);
    TEST_ASSERT_EQUAL(1, g_events);
    TEST_ASSERT_EQUAL(APP_EV_UP, g_last_event);

    app_helper_frame(&app);
    TEST_ASSERT_EQUAL(2, g_draws);

    app_helper_stop(&app);
}

static int g_ready;
static int g_ticks;

static void on_ready(app_helper_t* app) {
    g_ready++;
    TEST_ASSERT_EQUAL(1, app->ui.ctx.key_count);
    TEST_ASSERT_EQUAL_PTR(app, app->ui.ctx.keys[0].user);
}

static void on_tick(app_helper_t* app) {
    g_ticks++;
    if (g_ticks == 2) {
        app_helper_invalidate(app);
    }
}

static void custom_key(void* raw, void* user) {
    (void)raw;
    (void)user;
}

void test_custom_keys_game_mode_ready_and_live_frames(void) {
    static const app_ui_key_def_t keys[] = {
        {SIM_KEY_ESCAPE, custom_key, NULL},
        {0, NULL, NULL},
    };
    app_helper_desc_t desc = {
        .title = "GAME",
        .game = true,
        .live = true,
        .keys = keys,
        .on_ready = on_ready,
        .on_draw = on_draw,
    };
    app_helper_t app;
    g_ready = 0;
    g_draws = 0;

    TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
    TEST_ASSERT_EQUAL(1, g_ready);
    TEST_ASSERT_EQUAL(0, app.ui.ui.content_x);
    TEST_ASSERT_FALSE(app.ui.ui.show_top_bar);

    app_helper_frame(&app);
    app_helper_frame(&app);
    TEST_ASSERT_EQUAL(2, g_draws);
    app_helper_stop(&app);
}

void test_tick_can_request_another_draw(void) {
    app_helper_desc_t desc = {
        .title = "TICK",
        .on_tick = on_tick,
        .on_draw = on_draw,
    };
    app_helper_t app;
    g_ticks = 0;
    g_draws = 0;

    TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
    app_helper_frame(&app);
    app_helper_frame(&app);
    app_helper_frame(&app);
    TEST_ASSERT_EQUAL(3, g_ticks);
    TEST_ASSERT_EQUAL(2, g_draws);
    app_helper_stop(&app);
}

void test_center_text_ignores_a_missing_app_or_text(void) {
    app_helper_desc_t desc = {.title = "BOX"};
    app_helper_t app;
    app_helper_center_text(NULL, 0, 0, 10, 10, "A", 1, ARDUBOT_COLOR_TEXT);
    TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
    app_helper_center_text(&app, 0, 0, 40, 20, NULL, 1, ARDUBOT_COLOR_TEXT);
    app_helper_center_text(&app, 0, 0, 40, 20, "A", 0, ARDUBOT_COLOR_TEXT);
    app_helper_center_text(&app, 0, 0, 40, 20, "OK", 2, ARDUBOT_COLOR_TEXT);
    app_helper_stop(&app);
}

void test_start_rejects_a_missing_app_or_description(void) {
    app_helper_desc_t desc = {.title = "TEST"};
    app_helper_t app;
    TEST_ASSERT_EQUAL(-1, app_helper_start(NULL, NULL, &desc));
    TEST_ASSERT_EQUAL(-1, app_helper_start(&app, NULL, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fmt_clock_mmss);
    RUN_TEST(test_fmt_clock_clamps_negative_and_allows_long_minutes);
    RUN_TEST(test_fmt_clock_rejects_a_buffer_that_cannot_hold_the_text);
    RUN_TEST(test_fit_text_scale_clamps_to_the_given_max_and_at_least_one);
    RUN_TEST(test_center_in_places_the_item_inside_the_box);
    RUN_TEST(test_bar_fill_maps_and_clamps);
    RUN_TEST(test_gauge_fill_is_signed_and_clamped_to_the_range);
    RUN_TEST(test_level_color_uses_the_shared_palette);
    RUN_TEST(test_start_binds_the_standard_keys_and_draws_once_until_invalidated);
    RUN_TEST(test_custom_keys_game_mode_ready_and_live_frames);
    RUN_TEST(test_tick_can_request_another_draw);
    RUN_TEST(test_center_text_ignores_a_missing_app_or_text);
    RUN_TEST(test_start_rejects_a_missing_app_or_description);
    return UNITY_END();
}
