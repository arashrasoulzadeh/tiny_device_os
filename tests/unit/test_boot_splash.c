#include "app_helper.h"
#include "boot_splash.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_title_is_os_name_and_eight_hash_chars(void) {
  char buf[32];

  TEST_ASSERT_EQUAL(18, boot_splash_format_title(buf, sizeof(buf), "a1b2c3d4"));
  TEST_ASSERT_EQUAL_STRING("ArdubotOS a1b2c3d4", buf);

  TEST_ASSERT_EQUAL(
      18, boot_splash_format_title(buf, sizeof(buf), "0123456789abcdef"));
  TEST_ASSERT_EQUAL_STRING("ArdubotOS 89abcdef", buf);

  TEST_ASSERT_EQUAL(18, boot_splash_format_title(buf, sizeof(buf), NULL));
  TEST_ASSERT_EQUAL_STRING("ArdubotOS 00000000", buf);
  TEST_ASSERT_EQUAL(-1, boot_splash_format_title(buf, 8, "a1b2c3d4"));
  TEST_ASSERT_EQUAL(-1, boot_splash_format_title(NULL, 32, "a1b2c3d4"));
}

void test_status_is_the_step_label(void) {
  char buf[32];

  TEST_ASSERT_EQUAL(7, boot_splash_format_status(buf, sizeof(buf), "sensors"));
  TEST_ASSERT_EQUAL_STRING("sensors", buf);
  TEST_ASSERT_EQUAL(5, boot_splash_format_status(buf, sizeof(buf), "clock"));
  TEST_ASSERT_EQUAL_STRING("clock", buf);
  TEST_ASSERT_EQUAL(0, boot_splash_format_status(buf, sizeof(buf), NULL));
  TEST_ASSERT_EQUAL_STRING("", buf);
  TEST_ASSERT_EQUAL(-1, boot_splash_format_status(NULL, 8, "sensors"));
}

void test_bar_fill_matches_app_bar(void) {
  TEST_ASSERT_EQUAL(app_bar_fill_px(0, 4, 200),
                    boot_splash_bar_fill(0, 4, 200));
  TEST_ASSERT_EQUAL(app_bar_fill_px(1, 4, 200),
                    boot_splash_bar_fill(1, 4, 200));
  TEST_ASSERT_EQUAL(app_bar_fill_px(4, 4, 200),
                    boot_splash_bar_fill(4, 4, 200));
  TEST_ASSERT_EQUAL(0, boot_splash_bar_fill(1, 0, 200));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_title_is_os_name_and_eight_hash_chars);
  RUN_TEST(test_status_is_the_step_label);
  RUN_TEST(test_bar_fill_matches_app_bar);
  return UNITY_END();
}
