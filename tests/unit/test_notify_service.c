#include "display.h"
#include "notify_service.h"
#include "unity.h"

#include <string.h>

static void at(uint32_t ms) {
  notify_service_set_now_ms(ms);
  notify_service_pump();
}

static int post_card(const char *title, const char *body,
                     notify_extent_t extent, uint8_t band_percent,
                     uint32_t duration_ms) {
  notify_spec_t spec;
  memset(&spec, 0, sizeof(spec));
  spec.title = title;
  spec.body = body;
  spec.extent = extent;
  spec.band_percent = band_percent;
  spec.duration_ms = duration_ms;
  return notify_post(&spec);
}

static int text_line(void) {
  int scale = (APP_DISPLAY_HEIGHT > 64) ? 2 : 1;
  return 8 * scale;
}

static int default_band_h(void) {
  int h = (APP_DISPLAY_HEIGHT * NOTIFY_BAND_PERCENT) / 100;
  int min_h = text_line();
  if (h < min_h) {
    h = min_h;
  }
  if (h > APP_DISPLAY_HEIGHT) {
    h = APP_DISPLAY_HEIGHT;
  }
  return h;
}

void setUp(void) {
  TEST_ASSERT_EQUAL(0, notify_service_start());
  notify_service_set_now_ms(10000);
}

void tearDown(void) {}

void test_post_copies_title_and_rejects_a_full_queue(void) {
  char title[] = "saved";
  int i;
  int w = 0;
  int h = 0;

  TEST_ASSERT_EQUAL(-1, notify_post(NULL));
  TEST_ASSERT_EQUAL(-1, post_card("", NULL, NOTIFY_EXTENT_BAND, 0, 0));
  TEST_ASSERT_EQUAL(0, post_card(title, NULL, NOTIFY_EXTENT_BAND, 0, 0));
  title[0] = 'X';
  at(10000);
  TEST_ASSERT_EQUAL_STRING("saved", notify_service_title());

  for (i = 1; i < NOTIFY_QUEUE_CAP; i++) {
    TEST_ASSERT_EQUAL(0, post_card("queued", NULL, NOTIFY_EXTENT_FULL, 0, 0));
  }
  TEST_ASSERT_EQUAL(-1, post_card("overflow", NULL, NOTIFY_EXTENT_FULL, 0, 0));
  notify_service_card_rect(&w, &h);
  TEST_ASSERT_EQUAL(APP_DISPLAY_WIDTH, w);
  TEST_ASSERT_GREATER_THAN(0, h);
}

void test_band_and_full_extents(void) {
  int w = 0;
  int h = 0;

  TEST_ASSERT_EQUAL(0, post_card("band", "detail", NOTIFY_EXTENT_BAND, 0, 0));
  notify_service_card_rect(&w, &h);
  TEST_ASSERT_EQUAL(APP_DISPLAY_WIDTH, w);
  TEST_ASSERT_EQUAL(default_band_h(), h);

  TEST_ASSERT_EQUAL(0, notify_service_start());
  notify_service_set_now_ms(10000);
  TEST_ASSERT_EQUAL(0, post_card("full", NULL, NOTIFY_EXTENT_FULL, 10, 0));
  notify_service_card_rect(&w, &h);
  TEST_ASSERT_EQUAL(APP_DISPLAY_WIDTH, w);
  TEST_ASSERT_EQUAL(APP_DISPLAY_HEIGHT, h);

  TEST_ASSERT_EQUAL(0, notify_service_start());
  notify_service_set_now_ms(10000);
  TEST_ASSERT_EQUAL(0, post_card("thin", NULL, NOTIFY_EXTENT_BAND, 1, 0));
  notify_service_card_rect(&w, &h);
  TEST_ASSERT_EQUAL(text_line(), h);
}

void test_slide_and_auto_dismiss(void) {
  int h = 0;
  uint32_t t0 = 10000;
  uint32_t enter_end = t0 + NOTIFY_ANIM_MS;
  uint32_t hold_end = enter_end + NOTIFY_HOLD_MS;
  uint32_t exit_end = hold_end + NOTIFY_ANIM_MS;

  TEST_ASSERT_EQUAL(0, post_card("hello", "world", NOTIFY_EXTENT_FULL, 0, 0));
  at(t0);
  notify_service_card_rect(NULL, &h);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_ENTER, notify_service_phase());
  TEST_ASSERT_EQUAL(-h, notify_service_slide_offset());
  TEST_ASSERT_TRUE(notify_service_needs_present());

  at(enter_end - 1);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_ENTER, notify_service_phase());
  TEST_ASSERT_EQUAL(-h + (h * (int)(NOTIFY_ANIM_MS - 1)) / NOTIFY_ANIM_MS,
                    notify_service_slide_offset());

  at(enter_end);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_HOLD, notify_service_phase());
  TEST_ASSERT_EQUAL(0, notify_service_slide_offset());
  TEST_ASSERT_FALSE(notify_service_needs_present());

  at(hold_end - 1);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_HOLD, notify_service_phase());

  at(hold_end);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_EXIT, notify_service_phase());
  TEST_ASSERT_EQUAL(0, notify_service_slide_offset());
  TEST_ASSERT_TRUE(notify_service_needs_present());

  at(exit_end - 1);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_EXIT, notify_service_phase());
  TEST_ASSERT_EQUAL(-(h * (int)(NOTIFY_ANIM_MS - 1)) / NOTIFY_ANIM_MS,
                    notify_service_slide_offset());

  at(exit_end);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_IDLE, notify_service_phase());
  TEST_ASSERT_NULL(notify_service_title());
  TEST_ASSERT_FALSE(notify_service_needs_present());
}

void test_key_dismiss_before_the_hold_ends(void) {
  int h = 0;
  uint32_t t0 = 10000;

  TEST_ASSERT_EQUAL(0, post_card("press", NULL, NOTIFY_EXTENT_FULL, 0, 0));
  at(t0 + NOTIFY_ANIM_MS + 50);
  notify_service_card_rect(NULL, &h);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_HOLD, notify_service_phase());

  TEST_ASSERT_FALSE(notify_service_on_key(SIM_KEY_SYS_MENU, true));
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_HOLD, notify_service_phase());

  TEST_ASSERT_TRUE(notify_service_on_key(SIM_KEY_ENTER, false));
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_HOLD, notify_service_phase());

  TEST_ASSERT_TRUE(notify_service_on_key(SIM_KEY_ENTER, true));
  at(t0 + NOTIFY_ANIM_MS + 50);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_EXIT, notify_service_phase());
  TEST_ASSERT_EQUAL(0, notify_service_slide_offset());

  at(t0 + NOTIFY_ANIM_MS + 50 + 10);
  TEST_ASSERT_TRUE(notify_service_on_key(SIM_KEY_UP, true));
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_EXIT, notify_service_phase());

  at(t0 + NOTIFY_ANIM_MS + 50 + NOTIFY_ANIM_MS - 1);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_EXIT, notify_service_phase());
  TEST_ASSERT_EQUAL(-(h * (int)(NOTIFY_ANIM_MS - 1)) / NOTIFY_ANIM_MS,
                    notify_service_slide_offset());

  at(t0 + NOTIFY_ANIM_MS + 50 + NOTIFY_ANIM_MS);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_IDLE, notify_service_phase());
}

void test_key_is_ignored_when_idle(void) {
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_IDLE, notify_service_phase());
  TEST_ASSERT_FALSE(notify_service_on_key(SIM_KEY_ENTER, true));
  TEST_ASSERT_FALSE(notify_service_on_key(SIM_KEY_ENTER, false));
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_IDLE, notify_service_phase());
  TEST_ASSERT_FALSE(notify_service_needs_present());
}

void test_next_card_starts_after_hide(void) {
  uint32_t t0 = 10000;
  uint32_t exit_end = t0 + NOTIFY_ANIM_MS + NOTIFY_HOLD_MS + NOTIFY_ANIM_MS;
  int h = 0;

  TEST_ASSERT_EQUAL(0, post_card("first", NULL, NOTIFY_EXTENT_FULL, 0, 0));
  TEST_ASSERT_EQUAL(0, post_card("second", NULL, NOTIFY_EXTENT_BAND, 0, 0));
  at(t0 + 10);
  TEST_ASSERT_EQUAL_STRING("first", notify_service_title());
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_ENTER, notify_service_phase());

  at(exit_end - 1);
  TEST_ASSERT_EQUAL_STRING("first", notify_service_title());
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_EXIT, notify_service_phase());

  at(exit_end);
  TEST_ASSERT_EQUAL_STRING("second", notify_service_title());
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_ENTER, notify_service_phase());
  notify_service_card_rect(NULL, &h);
  TEST_ASSERT_EQUAL(-h, notify_service_slide_offset());
  TEST_ASSERT_EQUAL(default_band_h(), h);

  TEST_ASSERT_EQUAL(0, post_card("third", NULL, NOTIFY_EXTENT_FULL, 0, 0));
}

void test_custom_hold_starts_the_exit_early(void) {
  uint32_t t0 = 10000;
  uint32_t hold_end = t0 + NOTIFY_ANIM_MS + 500;

  TEST_ASSERT_EQUAL(0, post_card("short", NULL, NOTIFY_EXTENT_FULL, 0, 500));
  at(hold_end - 1);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_HOLD, notify_service_phase());
  at(hold_end);
  TEST_ASSERT_EQUAL(NOTIFY_PHASE_EXIT, notify_service_phase());
  TEST_ASSERT_EQUAL(0, notify_service_slide_offset());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_post_copies_title_and_rejects_a_full_queue);
  RUN_TEST(test_band_and_full_extents);
  RUN_TEST(test_slide_and_auto_dismiss);
  RUN_TEST(test_key_dismiss_before_the_hold_ends);
  RUN_TEST(test_key_is_ignored_when_idle);
  RUN_TEST(test_next_card_starts_after_hide);
  RUN_TEST(test_custom_hold_starts_the_exit_early);
  return UNITY_END();
}
