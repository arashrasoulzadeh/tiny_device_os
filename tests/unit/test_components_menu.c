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
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "a", "Alpha", "[USR]", NULL));
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "b", "Beta", "[TOL]", NULL));
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "c", "Gamma", "[GME]", NULL));
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
    TEST_ASSERT_TRUE(app_menu_move(&ctx, &menu, APP_MENU_ONE_UP, NULL));
    TEST_ASSERT_EQUAL(2, menu.selected);
    TEST_ASSERT_TRUE(app_is_dirty(&ctx));
}

void test_menu_add_rejects_overflow(void) {
    app_menu_t menu;
    app_menu_init(&menu, 16, 12);
    for (int i = 0; i < APP_KIT_MENU_MAX; i++) {
        TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "id", "label", NULL, NULL));
    }
    TEST_ASSERT_NOT_EQUAL(0, app_menu_add(&menu, "extra", "x", NULL, NULL));
}

void test_app_display_matches_device_panel(void) {
    TEST_ASSERT_EQUAL(128, APP_DISPLAY_WIDTH);
    TEST_ASSERT_EQUAL(32, APP_DISPLAY_HEIGHT);
}

void test_menu_icon_strip_unlimited_wrap(void) {
    app_ctx_t ctx;
    app_menu_t menu;
    memset(&ctx, 0, sizeof(ctx));
    app_menu_init(&menu, 8, 8);
    app_menu_set_icon_strip(&menu);
    TEST_ASSERT_EQUAL(APP_MENU_LAYOUT_ICONS, menu.layout);
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "counter", "counter", NULL, NULL));
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "info", "info", NULL, NULL));
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "stopwatch", "stopwatch", NULL, NULL));

    menu.selected = 2;
    TEST_ASSERT_TRUE(app_menu_move(&ctx, &menu, 1, NULL));
    TEST_ASSERT_EQUAL(0, menu.selected);

    TEST_ASSERT_TRUE(app_menu_move(&ctx, &menu, -1, NULL));
    TEST_ASSERT_EQUAL(2, menu.selected);
}

void test_app_provides_icon_via_kit(void) {
    static const app_icon_t icon = {{
        0xFFFF, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001,
        0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0xFFFF,
    }};
    app_kit_set_icon("demo", &icon);
    TEST_ASSERT_EQUAL_PTR(&icon, app_kit_get_icon("demo"));
    TEST_ASSERT_EQUAL_PTR(&app_icon_default, app_kit_get_icon("missing"));
}

void test_app_menu_bind_nav_registers_arrows(void) {
    app_ctx_t ctx;
    app_menu_t menu;
    memset(&ctx, 0, sizeof(ctx));
    app_menu_init(&menu, 8, 8);
    app_menu_set_icon_strip(&menu);
    TEST_ASSERT_EQUAL(0, app_menu_bind_nav(&ctx, &menu));
    TEST_ASSERT_EQUAL(4, (int)ctx.key_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_menu_move_and_select);
    RUN_TEST(test_menu_add_rejects_overflow);
    RUN_TEST(test_app_display_matches_device_panel);
    RUN_TEST(test_menu_icon_strip_unlimited_wrap);
    RUN_TEST(test_app_provides_icon_via_kit);
    RUN_TEST(test_app_menu_bind_nav_registers_arrows);
    return UNITY_END();
}
