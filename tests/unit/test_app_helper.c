#include "app_helper.h"
#include "app_kit.h"
#include "header.h"
#include "power.h"
#include "scheduler.h"
#include "theme.h"
#include "unity.h"

#include <string.h>

void setUp(void) {
  scheduler_init();
  power_init();
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

static void on_event(app_helper_t *app, app_helper_event_t ev) {
  (void)app;
  g_events++;
  g_last_event = ev;
}

static void on_draw(app_helper_t *app) {
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
  TEST_ASSERT_EQUAL(8, app.ui.ctx->key_count);
  TEST_ASSERT_EQUAL(SIM_KEY_DOWN, app.ui.ctx->keys[2].key);
  TEST_ASSERT_EQUAL(SIM_KEY_LEFT, app.ui.ctx->keys[3].key);
  TEST_ASSERT_EQUAL(SIM_KEY_RIGHT, app.ui.ctx->keys[4].key);

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

static void on_ready(app_helper_t *app) {
  g_ready++;
  TEST_ASSERT_EQUAL(1, app->ui.ctx->key_count);
  TEST_ASSERT_EQUAL_PTR(app, app->ui.ctx->keys[0].user);
}

static void on_tick(app_helper_t *app) {
  g_ticks++;
  if (g_ticks == 2) {
    app_helper_invalidate(app);
  }
}

static void custom_key(void *raw, void *user) {
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
  TEST_ASSERT_EQUAL(0, app.ui.ctx->ui.content_x);
  TEST_ASSERT_FALSE(app.ui.ctx->ui.show_top_bar);

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

void test_omitted_title_and_help_come_from_app_json(void) {
  app_helper_desc_t desc = {.name = "counter"};
  app_helper_t app;
  TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
  TEST_ASSERT_EQUAL_STRING("COUNTER", app.ui.ctx->ui.title);
  TEST_ASSERT_EQUAL_STRING("Up:+  Sel:-  hold:back", app.ui.ctx->ui.help_text);
  app_helper_stop(&app);
}

void test_explicit_title_and_help_override_app_json(void) {
  app_helper_desc_t desc = {
      .name = "counter", .title = "LOCAL", .help = "local help"};
  app_helper_t app;
  TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
  TEST_ASSERT_EQUAL_STRING("LOCAL", app.ui.ctx->ui.title);
  TEST_ASSERT_EQUAL_STRING("local help", app.ui.ctx->ui.help_text);
  app_helper_stop(&app);
}

static power_demand_t g_helper_demand;

static void on_init_demand(void *raw) {
  app_helper_desc_t desc = {
      .name = "clock",
      .title = "C",
      .demand = POWER_DEMAND_LOW,
  };
  app_helper_t helper;
  TEST_ASSERT_EQUAL(0, app_helper_start(&helper, raw, &desc));
  g_helper_demand = power_governor_get_foreground_demand();
  app_helper_stop(&helper);
  ((app_ctx_t *)raw)->running = false;
}

void test_helper_demand_is_published_for_the_foreground_app(void) {
  app_desc_t desc = {
      .name = "clock-demand",
      .fps = 30,
      .on_init = on_init_demand,
  };
  g_helper_demand = POWER_DEMAND_UNSET;
  app_kit_run(&desc);
  TEST_ASSERT_EQUAL(POWER_DEMAND_LOW, g_helper_demand);
}

void test_fullscreen_drops_the_launcher_header(void) {
  app_helper_desc_t desc = {
      .title = "FS",
      .fullscreen = true,
  };
  app_helper_t app;

  TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
  TEST_ASSERT_EQUAL(APP_UI_MODE_GAME, app.ui.ctx->ui.mode);
  TEST_ASSERT_EQUAL(0, app.ui.ctx->ui.content_x);
  TEST_ASSERT_EQUAL(0, app.ui.ctx->ui.content_y);
  TEST_ASSERT_FALSE(app.ui.ctx->ui.show_top_bar);
  app_helper_stop(&app);
}

static int g_kit_ticks;
static int g_kit_draws;
static app_helper_t g_kit_helper;

static void kit_tick(app_helper_t *app) {
  g_kit_ticks++;
  if (g_kit_ticks >= 1) {
    app->ui.ctx->running = false;
  }
}

static void kit_draw(app_helper_t *app) {
  (void)app;
  g_kit_draws++;
}

static void kit_init(void *raw) {
  static const app_helper_desc_t desc = {
      .title = "T",
      .every_ms = 200,
      .on_tick = kit_tick,
      .on_draw = kit_draw,
  };
  TEST_ASSERT_EQUAL(0, app_helper_start(&g_kit_helper, raw, &desc));
}

static void kit_frame(void *raw) {
  (void)raw;
  app_helper_frame(&g_kit_helper);
}

void test_every_ms_ticks_on_that_interval_and_skips_a_clean_draw(void) {
  app_desc_t desc = {
      .name = "every",
      .fps = 30,
      .on_init = kit_init,
      .on_frame = kit_frame,
  };
  uint32_t start;
  g_kit_ticks = 0;
  g_kit_draws = 0;
  start = scheduler_get_tick_count();
  app_kit_run(&desc);
  TEST_ASSERT_EQUAL(1, g_kit_ticks);
  TEST_ASSERT_EQUAL(1, g_kit_draws);
  TEST_ASSERT_GREATER_OR_EQUAL(200, scheduler_get_tick_count() - start);
}

void test_scene_rows_follow_the_hero_and_the_meter(void) {
  TEST_ASSERT_EQUAL(0, app_scene_row_y(0, 16, 0));
  TEST_ASSERT_EQUAL(16, app_scene_row_y(1, 16, 0));
  TEST_ASSERT_EQUAL(34, app_scene_block_h(4, false));
  TEST_ASSERT_EQUAL(48, app_scene_block_h(4, true));
  TEST_ASSERT_EQUAL(48, app_scene_row_y(0, 16, app_scene_block_h(4, true)));
  TEST_ASSERT_EQUAL(64, app_scene_row_y(1, 16, app_scene_block_h(4, true)));
}

void test_scene_hero_scale_fits_beside_a_panel(void) {
  int full = app_scene_hero_scale(320, 5, 6, false);
  int beside = app_scene_hero_scale(320, 5, 6, true);
  TEST_ASSERT_EQUAL(app_fit_text_scale(320, 5, 6), full);
  TEST_ASSERT_EQUAL(app_fit_text_scale(320 - app_scene_panel_w(320), 5, 6),
                    beside);
  TEST_ASSERT_EQUAL(128, app_scene_panel_w(320));
  TEST_ASSERT_TRUE(beside <= full);
}

void test_scene_bar_fill_uses_the_value(void) {
  app_helper_t app;
  memset(&app, 0, sizeof(app));
  app_scene_bar(&app, 50, 100, ARDUBOT_COLOR_SUCCESS);
  TEST_ASSERT_TRUE(app.scene.bar_set);
  TEST_ASSERT_EQUAL(
      100, app_bar_fill_px(app.scene.bar_value, app.scene.bar_total, 200));
  app_scene_row(&app, 0, "one");
  app_scene_row(&app, 2, "three");
  TEST_ASSERT_EQUAL(3, app.scene.row_count);
  TEST_ASSERT_EQUAL_STRING("one", app.scene.rows[0]);
  TEST_ASSERT_EQUAL_STRING("three", app.scene.rows[2]);
}

void test_a_retired_app_does_not_paint(void) {
  app_helper_desc_t desc = {.title = "OLD", .on_draw = on_draw};
  app_helper_t app;
  g_draws = 0;
  TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
  app.ui.ctx->retired = true;
  app_helper_frame(&app);
  TEST_ASSERT_EQUAL(0, g_draws);
  app_helper_stop(&app);
}

void test_start_rejects_a_missing_app_or_description(void) {
  app_helper_desc_t desc = {.title = "TEST"};
  app_helper_t app;
  TEST_ASSERT_EQUAL(-1, app_helper_start(NULL, NULL, &desc));
  TEST_ASSERT_EQUAL(-1, app_helper_start(&app, NULL, NULL));
}

/* The header clock moves every second. The body has to be painted in
 * that same frame, or the app sits still under a ticking band. */
void test_header_second_redraws_the_app_body(void) {
  app_helper_desc_t desc = {.title = "T", .on_draw = on_draw};
  app_helper_t app;
  int i;
  if (app_header_height() <= 0) {
    TEST_IGNORE_MESSAGE("panel is too small for the header band");
    return;
  }
  g_draws = 0;
  TEST_ASSERT_EQUAL(0, app_helper_start(&app, NULL, &desc));
  app_helper_frame(&app);
  app_helper_frame(&app);
  TEST_ASSERT_EQUAL(1, g_draws);
  for (i = 0; i < 1000; i++) {
    scheduler_tick();
  }
  app_helper_frame(&app);
  TEST_ASSERT_EQUAL(2, g_draws);
  app_helper_stop(&app);
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
  RUN_TEST(test_omitted_title_and_help_come_from_app_json);
  RUN_TEST(test_explicit_title_and_help_override_app_json);
  RUN_TEST(test_helper_demand_is_published_for_the_foreground_app);
  RUN_TEST(test_fullscreen_drops_the_launcher_header);
  RUN_TEST(test_scene_rows_follow_the_hero_and_the_meter);
  RUN_TEST(test_scene_hero_scale_fits_beside_a_panel);
  RUN_TEST(test_scene_bar_fill_uses_the_value);
  RUN_TEST(test_every_ms_ticks_on_that_interval_and_skips_a_clean_draw);
  RUN_TEST(test_a_retired_app_does_not_paint);
  RUN_TEST(test_start_rejects_a_missing_app_or_description);
  RUN_TEST(test_header_second_redraws_the_app_body);
  return UNITY_END();
}
