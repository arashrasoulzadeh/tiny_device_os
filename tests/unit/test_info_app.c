#include "unity.h"
#include "device_info.h"
#include <string.h>

/* Smoke coverage for apps/stdapps/info_app.c (TDD gate). */
void setUp(void) {}
void tearDown(void) {}

void test_info_app_device_snapshot_is_printable(void) {
    device_info_t info;
    TEST_ASSERT_EQUAL(0, device_info_query(&info));
    TEST_ASSERT_NOT_NULL(info.os_name);
    TEST_ASSERT_NOT_NULL(info.target);
    TEST_ASSERT_TRUE(info.display_w >= 64);
    TEST_ASSERT_TRUE(info.display_h >= 32);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_info_app_device_snapshot_is_printable);
    return UNITY_END();
}
