#include "unity.h"
#include "app_kit.h"
#include "menu.h"
#include "scheduler.h"
#include <string.h>

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_menu_move_and_select(void) {
    app_ctx_t ctx;
    app_menu_t menu;
    memset(&ctx, 0, sizeof(ctx));
    app_menu_init(&menu, 16, 12);
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "a", "Alpha", "[USR]"));
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "b", "Beta", "[TOL]"));
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "c", "Gamma", "[GME]"));
    TEST_ASSERT_EQUAL(3, menu.count);
    TEST_ASSERT_EQUAL(0, menu.selected);

    TEST_ASSERT_TRUE(app_menu_move(&ctx, &menu, APP_MENU_ONE_DOWN, NULL));
    TEST_ASSERT_EQUAL(1, menu.selected);
    TEST_ASSERT_EQUAL_STRING("b", app_menu_selected(&menu)->id);
    TEST_ASSERT_TRUE(app_is_dirty(&ctx));

    app_clear_dirty(&ctx);
    TEST_ASSERT_TRUE(app_menu_move(&ctx, &menu, APP_MENU_ONE_UP, NULL));
    TEST_ASSERT_EQUAL(0, menu.selected);
    TEST_ASSERT_TRUE(app_is_dirty(&ctx));

    app_clear_dirty(&ctx);
    TEST_ASSERT_FALSE(app_menu_move(&ctx, &menu, APP_MENU_ONE_UP, NULL));
    TEST_ASSERT_FALSE(app_is_dirty(&ctx));
}

void test_menu_add_rejects_overflow(void) {
    app_menu_t menu;
    app_menu_init(&menu, 16, 12);
    for (int i = 0; i < APP_KIT_MENU_MAX; i++) {
        TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "id", "label", NULL));
    }
    TEST_ASSERT_NOT_EQUAL(0, app_menu_add(&menu, "extra", "x", NULL));
}

void test_app_display_height_is_positive(void) {
    TEST_ASSERT_TRUE(APP_DISPLAY_HEIGHT > 0);
    TEST_ASSERT_TRUE(APP_DISPLAY_WIDTH > 0);
}

void test_app_menu_bind_nav_registers_up_down(void) {
    app_ctx_t ctx;
    app_menu_t menu;
    memset(&ctx, 0, sizeof(ctx));
    app_menu_init(&menu, 16, 12);
    TEST_ASSERT_EQUAL(0, app_menu_bind_nav(&ctx, &menu));
    TEST_ASSERT_EQUAL(2, (int)ctx.key_count);
    TEST_ASSERT_EQUAL(SIM_KEY_UP, ctx.keys[0].key);
    TEST_ASSERT_EQUAL(SIM_KEY_DOWN, ctx.keys[1].key);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_menu_move_and_select);
    RUN_TEST(test_menu_add_rejects_overflow);
    RUN_TEST(test_app_display_height_is_positive);
    RUN_TEST(test_app_menu_bind_nav_registers_up_down);
    return UNITY_END();
}
