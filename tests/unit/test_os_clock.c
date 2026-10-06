#include "unity.h"
#include "os_clock.h"

void setUp(void) {
    os_clock_reset();
}

void tearDown(void) {
    os_clock_reset();
}

void test_stamp_keeps_a_clock_that_is_already_set(void) {
    TEST_ASSERT_EQUAL_INT(0, (int)os_clock_stamp(1700000000, 1700001111));
}

void test_stamp_uses_the_compiled_time_when_the_rtc_is_unset(void) {
    TEST_ASSERT_EQUAL_INT(1700001111, (int)os_clock_stamp(0, 1700001111));
    TEST_ASSERT_EQUAL_INT(1700001111, (int)os_clock_stamp(1000, 1700001111));
}

void test_stamp_does_nothing_without_a_compiled_time(void) {
    TEST_ASSERT_EQUAL_INT(0, (int)os_clock_stamp(0, 0));
    TEST_ASSERT_EQUAL_INT(0, (int)os_clock_stamp(0, 1000));
}

void test_tz_string_matches_posix_sign(void) {
    char buf[16];

    TEST_ASSERT_EQUAL(0, os_clock_tz_string(210, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("UTC-3:30", buf);
    TEST_ASSERT_EQUAL(0, os_clock_tz_string(-300, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("UTC+5:00", buf);
    TEST_ASSERT_EQUAL(0, os_clock_tz_string(0, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("UTC0", buf);
    TEST_ASSERT_EQUAL(-1, os_clock_tz_string(210, NULL, 16));
    TEST_ASSERT_EQUAL(-1, os_clock_tz_string(210, buf, 4));
}

void test_set_moves_the_host_clock_to_an_absolute_time(void) {
    TEST_ASSERT_EQUAL(0, os_clock_set(1700000000));
    TEST_ASSERT_INT_WITHIN(2, 1700000000, (int)os_clock_now());
    os_clock_reset();
}

void test_shift_moves_the_host_clock_without_touching_the_system(void) {
    time_t before = os_clock_now();
    TEST_ASSERT_EQUAL(0, os_clock_shift(3600));
    TEST_ASSERT_EQUAL(0, os_clock_shift(60));
    TEST_ASSERT_INT_WITHIN(2, (int)(before + 3660), (int)os_clock_now());
    os_clock_reset();
    TEST_ASSERT_INT_WITHIN(2, (int)before, (int)os_clock_now());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stamp_keeps_a_clock_that_is_already_set);
    RUN_TEST(test_stamp_uses_the_compiled_time_when_the_rtc_is_unset);
    RUN_TEST(test_stamp_does_nothing_without_a_compiled_time);
    RUN_TEST(test_tz_string_matches_posix_sign);
    RUN_TEST(test_set_moves_the_host_clock_to_an_absolute_time);
    RUN_TEST(test_shift_moves_the_host_clock_without_touching_the_system);
    return UNITY_END();
}
