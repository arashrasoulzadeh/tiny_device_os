#include "unity.h"
#include "device_info.h"
#include "scheduler.h"
#include "app.h"
#include <string.h>

void setUp(void) {
    scheduler_init();
    app_init();
}

void tearDown(void) {
    app_deinit();
}

void test_device_info_query_fills_identity_fields(void) {
    device_info_t info;
    memset(&info, 0, sizeof(info));

    TEST_ASSERT_EQUAL(0, device_info_query(&info));
    TEST_ASSERT_NOT_NULL(info.os_name);
    TEST_ASSERT_TRUE(strlen(info.os_name) > 0);
    TEST_ASSERT_NOT_NULL(info.os_version);
    TEST_ASSERT_TRUE(strlen(info.os_version) > 0);
    TEST_ASSERT_NOT_NULL(info.target);
    TEST_ASSERT_NOT_NULL(info.arch);
    TEST_ASSERT_TRUE(info.display_w > 0);
    TEST_ASSERT_TRUE(info.display_h > 0);
}

void test_device_info_query_null_fails(void) {
    TEST_ASSERT_NOT_EQUAL(0, device_info_query(NULL));
}

void test_device_info_uptime_tracks_ticks(void) {
    device_info_t before;
    device_info_t after;
    TEST_ASSERT_EQUAL(0, device_info_query(&before));
    scheduler_tick();
    scheduler_tick();
    TEST_ASSERT_EQUAL(0, device_info_query(&after));
    TEST_ASSERT_TRUE(after.uptime_ms >= before.uptime_ms + 2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_device_info_query_fills_identity_fields);
    RUN_TEST(test_device_info_query_null_fails);
    RUN_TEST(test_device_info_uptime_tracks_ticks);
    return UNITY_END();
}
