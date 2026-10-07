#include "app_framework.h"
#include "fw/ui.h"
#include "header.h"
#include "scheduler.h"
#include "sim_i2c.h"
#include "sim_video.h"
#include "unity.h"

void setUp(void) { scheduler_init(); }

void tearDown(void) {}

void test_header_height_follows_the_panel(void) {
#if APP_DISPLAY_WIDTH >= APP_HEADER_MIN_WIDTH &&                               \
    APP_DISPLAY_HEIGHT >= APP_HEADER_MIN_HEIGHT
  TEST_ASSERT_EQUAL(APP_HEADER_BAND, app_header_height());
#else
  TEST_ASSERT_EQUAL(0, app_header_height());
#endif
}

static uint32_t pixel_at(int x, int y) {
  uint32_t *pixels = sim_video_get_pixels();
  int sim_w = sim_video_get_width();
  if (!pixels || x < 0 || y < 0 || x >= sim_w) {
    return 0;
  }
  return pixels[y * sim_w + x];
}

/* The band is painted after the app, so a fill across the top row does
 * not stick on a normal screen. A game keeps that fill. */
void test_header_stays_on_top_of_a_ui_app_and_stays_off_a_game(void) {
  app_ui_config_t cfg;
  app_ui_t ui;
  const uint16_t red = APP_UI_RGB565(255, 0, 0);

  if (app_header_height() <= 0) {
    TEST_IGNORE_MESSAGE("panel is too small for the launcher header");
    return;
  }

  sim_i2c_init();
  if (sim_video_init(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT, "header") != 0 ||
      !sim_video_get_pixels()) {
    sim_i2c_cleanup();
    TEST_IGNORE_MESSAGE("SDL video unavailable (headless / no display)");
    return;
  }

  app_ui_config_ui(&cfg, "TEST", "help");
  TEST_ASSERT_EQUAL(0, app_ui_init(&ui, NULL, &cfg));
  app_ui_begin_frame(&ui);
  app_display_fill_rect_color(&ui.ctx->display, 0, 0, APP_DISPLAY_WIDTH,
                              app_header_height(), 0, red);
  app_ui_end_frame(&ui);
  /* Gap beside the battery is the header background, not the app's red. */
  TEST_ASSERT_EQUAL_UINT32(0xFF000000u, pixel_at(2, 2));
  /* Top edge of the battery outline. */
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, pixel_at(19, 5));
  app_ui_deinit(&ui);

  app_ui_config_game(&cfg);
  TEST_ASSERT_EQUAL(0, app_ui_init(&ui, NULL, &cfg));
  app_ui_begin_frame(&ui);
  app_display_fill_rect_color(&ui.ctx->display, 0, 0, APP_DISPLAY_WIDTH,
                              APP_HEADER_BAND, 0, red);
  app_ui_end_frame(&ui);
  TEST_ASSERT_EQUAL_UINT32(0xFFFF0000u, pixel_at(19, 5));
  app_ui_deinit(&ui);

  sim_video_cleanup();
  sim_i2c_cleanup();
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_header_height_follows_the_panel);
  RUN_TEST(test_header_stays_on_top_of_a_ui_app_and_stays_off_a_game);
  return UNITY_END();
}
