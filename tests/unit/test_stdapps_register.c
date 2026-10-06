#include "unity.h"
#include "app.h"
#include "stdapps_register.h"

void setUp(void) {
    app_init();
}

void tearDown(void) {
    app_deinit();
}

void test_stdapps_install_registers_only_apps_folder_builtins(void) {
    TEST_ASSERT_EQUAL(0, stdapps_install());
    TEST_ASSERT_NOT_NULL(app_find("launcher"));
    TEST_ASSERT_NOT_NULL(app_find("counter"));
    TEST_ASSERT_NOT_NULL(app_find("info"));
    TEST_ASSERT_NOT_NULL(app_find("stopwatch"));
    TEST_ASSERT_NOT_NULL(app_find("pong"));
    TEST_ASSERT_NOT_NULL(app_find("widgets"));
    TEST_ASSERT_NOT_NULL(app_find("pomodoro"));
    TEST_ASSERT_NOT_NULL(app_find("taskmgr"));
    TEST_ASSERT_NOT_NULL(app_find("clock"));
    /* Linked sources that are not part of the builtin catalog. */
    TEST_ASSERT_NULL(app_find("settings"));
    TEST_ASSERT_NULL(app_find("fileman"));
    TEST_ASSERT_NULL(app_find("shell"));
    TEST_ASSERT_NULL(app_find("demo"));
    TEST_ASSERT_EQUAL_STRING("info", stdapps_start_name());
}

void test_stdapps_install_rejects_a_second_pass(void) {
    TEST_ASSERT_EQUAL(0, stdapps_install());
    TEST_ASSERT_EQUAL(-1, stdapps_install());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stdapps_install_registers_only_apps_folder_builtins);
    RUN_TEST(test_stdapps_install_rejects_a_second_pass);
    return UNITY_END();
}
