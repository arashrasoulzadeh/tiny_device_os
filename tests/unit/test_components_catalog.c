#include "unity.h"
#include "app_kit.h"
#include "catalog.h"
#include "menu.h"
#include "scheduler.h"

static void dummy_entry(void) {}

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_catalog_build_excludes_home_and_loads_menu(void) {
    app_menu_t menu;
    app_manifest_t* counter = NULL;
    app_manifest_t* info = NULL;
    app_manifest_t* launcher = NULL;
    app_desc_t d_counter = {.name = "counter", .type = APP_TYPE_USER, .version = "1"};
    app_desc_t d_info = {.name = "info", .type = APP_TYPE_TOOL, .version = "1"};
    app_desc_t d_launch = {.name = "launcher", .type = APP_TYPE_SYSTEM, .version = "1"};

    app_init();
    app_kit_catalog_clear();

    counter = app_kit_make_manifest(&d_counter, dummy_entry);
    info = app_kit_make_manifest(&d_info, dummy_entry);
    launcher = app_kit_make_manifest(&d_launch, dummy_entry);
    TEST_ASSERT_NOT_NULL(counter);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_NOT_NULL(launcher);

    TEST_ASSERT_EQUAL(0, app_install_manifest(counter, "counter"));
    TEST_ASSERT_EQUAL(0, app_install_manifest(info, "info"));
    TEST_ASSERT_EQUAL(0, app_install_manifest(launcher, "launcher"));

    TEST_ASSERT_EQUAL(2, app_kit_catalog_build("launcher"));
    TEST_ASSERT_EQUAL(2, app_kit_catalog_count());
    TEST_ASSERT_NOT_NULL(app_kit_catalog_at(0));
    TEST_ASSERT_EQUAL_STRING("[TOL]", app_type_tag(APP_TYPE_TOOL));

    app_menu_init(&menu, 16, 12);
    TEST_ASSERT_EQUAL(2, app_menu_load_catalog(&menu));
    TEST_ASSERT_EQUAL(2, app_menu_load_catalog(&menu));
}

void test_app_type_tag_values(void) {
    TEST_ASSERT_EQUAL_STRING("[SYS]", app_type_tag(APP_TYPE_SYSTEM));
    TEST_ASSERT_EQUAL_STRING("[USR]", app_type_tag(APP_TYPE_USER));
    TEST_ASSERT_EQUAL_STRING("[GME]", app_type_tag(APP_TYPE_GAME));
    TEST_ASSERT_EQUAL_STRING("[TOL]", app_type_tag(APP_TYPE_TOOL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_catalog_build_excludes_home_and_loads_menu);
    RUN_TEST(test_app_type_tag_values);
    return UNITY_END();
}
