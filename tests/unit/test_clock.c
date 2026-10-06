#include "unity.h"
#include "clock.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_hms_splits_a_day_and_wraps_after_midnight(void) {
    int h = -1;
    int m = -1;
    int s = -1;
    clock_hms(0, &h, &m, &s);
    TEST_ASSERT_EQUAL(0, h);
    TEST_ASSERT_EQUAL(0, m);
    TEST_ASSERT_EQUAL(0, s);

    clock_hms(3661, &h, &m, &s);
    TEST_ASSERT_EQUAL(1, h);
    TEST_ASSERT_EQUAL(1, m);
    TEST_ASSERT_EQUAL(1, s);

    clock_hms(86400u + 3661u, &h, &m, &s);
    TEST_ASSERT_EQUAL(1, h);
    TEST_ASSERT_EQUAL(1, m);
    TEST_ASSERT_EQUAL(1, s);
}

void test_fmt_hms_writes_a_24_hour_clock(void) {
    char buf[9];
    TEST_ASSERT_EQUAL(8, clock_fmt_hms(buf, sizeof(buf), 9, 5, 3));
    TEST_ASSERT_EQUAL_STRING("09:05:03", buf);
    TEST_ASSERT_EQUAL(8, clock_fmt_hms(buf, sizeof(buf), 26, -1, 70));
    TEST_ASSERT_EQUAL_STRING("02:59:10", buf);
    TEST_ASSERT_EQUAL(-1, clock_fmt_hms(buf, 4, 1, 2, 3));
    TEST_ASSERT_EQUAL(-1, clock_fmt_hms(NULL, 8, 1, 2, 3));
}

void test_hands_point_at_the_cardinal_hours(void) {
    int x = 0;
    int y = 0;
    TEST_ASSERT_EQUAL(0, clock_hour_sixtieths(12, 0));
    TEST_ASSERT_EQUAL(15, clock_hour_sixtieths(3, 0));
    TEST_ASSERT_EQUAL(30, clock_hour_sixtieths(6, 0));
    TEST_ASSERT_EQUAL(45, clock_hour_sixtieths(9, 0));
    TEST_ASSERT_EQUAL(32, clock_hour_sixtieths(6, 24));
    TEST_ASSERT_EQUAL(15, clock_minute_sixtieths(15));
    TEST_ASSERT_EQUAL(42, clock_second_sixtieths(42));

    clock_hand_end(50, 50, 10, 0, &x, &y);
    TEST_ASSERT_EQUAL(50, x);
    TEST_ASSERT_EQUAL(40, y);
    clock_hand_end(50, 50, 10, 15, &x, &y);
    TEST_ASSERT_EQUAL(60, x);
    TEST_ASSERT_EQUAL(50, y);
    clock_hand_end(50, 50, 10, 30, &x, &y);
    TEST_ASSERT_EQUAL(50, x);
    TEST_ASSERT_EQUAL(60, y);
    clock_hand_end(50, 50, 10, 45, &x, &y);
    TEST_ASSERT_EQUAL(40, x);
    TEST_ASSERT_EQUAL(50, y);
    clock_hand_end(50, 50, 10, -15, &x, &y);
    TEST_ASSERT_EQUAL(40, x);
    TEST_ASSERT_EQUAL(50, y);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hms_splits_a_day_and_wraps_after_midnight);
    RUN_TEST(test_fmt_hms_writes_a_24_hour_clock);
    RUN_TEST(test_hands_point_at_the_cardinal_hours);
    return UNITY_END();
}
