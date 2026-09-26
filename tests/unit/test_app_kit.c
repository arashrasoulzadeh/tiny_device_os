#include "unity.h"
#include "app_kit.h"
#include "scheduler.h"
#include <string.h>

static void dummy_entry(void) {}

static void dummy_key(app_ctx_t* app, void* user) {
    (void)app;
    (void)user;
}

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_app_kit_make_manifest_fills_fields(void) {
    app_desc_t desc = {
        .name = "demo",
        .version = "1.2.3",
        .author = "Tester",
        .description = "Demo app",
        .type = APP_TYPE_TOOL,
        .fps = 30,
        .stack_size = APP_STACK_MEDIUM,
        .heap_size = APP_HEAP_SMALL,
        .on_init = NULL,
        .on_frame = NULL,
        .on_cleanup = NULL,
    };

    app_manifest_t* m = app_kit_make_manifest(&desc, dummy_entry);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_STRING("demo", m->name);
    TEST_ASSERT_EQUAL_STRING("1.2.3", m->version);
    TEST_ASSERT_EQUAL_STRING("Tester", m->author);
    TEST_ASSERT_EQUAL_STRING("Demo app", m->description);
    TEST_ASSERT_EQUAL(APP_TYPE_TOOL, m->type);
    TEST_ASSERT_EQUAL((uintptr_t)dummy_entry, m->entry_point);
    TEST_ASSERT_EQUAL(APP_STACK_MEDIUM, m->stack_size);
    TEST_ASSERT_EQUAL(APP_HEAP_SMALL, m->heap_size);
    TEST_ASSERT_TRUE(m->capability_count >= 1);
}

void test_app_dirty_helpers(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    TEST_ASSERT_FALSE(app_is_dirty(&ctx));
    app_mark_dirty(&ctx);
    TEST_ASSERT_TRUE(app_is_dirty(&ctx));
    app_clear_dirty(&ctx);
    TEST_ASSERT_FALSE(app_is_dirty(&ctx));
}

void test_app_textf_and_flush_do_not_crash(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    TEST_ASSERT_EQUAL(0, app_display_init(&ctx.display, "/dev/display0"));

    /* Not foreground: draw helpers are no-ops but must not crash. */
    app_clear(&ctx);
    app_text(&ctx, 0, 0, "Hello");
    app_textf(&ctx, 0, 8, "n=%d", 42);
    app_flush(&ctx);
    app_mark_dirty(&ctx);
    TEST_ASSERT_TRUE(app_is_dirty(&ctx));
}

void test_app_request_exit_stops_running_flag(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.running = true;
    app_request_exit(&ctx);
    TEST_ASSERT_FALSE(ctx.running);
}

void test_app_kit_is_foreground_false_without_focus(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    TEST_ASSERT_FALSE(app_kit_is_foreground(&ctx));
    TEST_ASSERT_FALSE(app_kit_is_foreground(NULL));
}

void test_app_bind_key_succeeds_then_rejects_overflow(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    TEST_ASSERT_EQUAL(0, app_bind_key(&ctx, SIM_KEY_1, dummy_key, NULL));
    TEST_ASSERT_EQUAL(0, app_bind_key(&ctx, SIM_KEY_2, dummy_key, NULL));
    TEST_ASSERT_EQUAL(2, (int)ctx.key_count);

    /* Fill remaining slots */
    sim_key_t keys[] = {SIM_KEY_3, SIM_KEY_4, SIM_KEY_5, SIM_KEY_6, SIM_KEY_7, SIM_KEY_8};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        TEST_ASSERT_EQUAL(0, app_bind_key(&ctx, keys[i], dummy_key, NULL));
    }
    TEST_ASSERT_EQUAL(APP_KIT_MAX_KEYS, (int)ctx.key_count);
    TEST_ASSERT_NOT_EQUAL(0, app_bind_key(&ctx, SIM_KEY_9, dummy_key, NULL));
}

void test_app_menu_move_and_select(void) {
    app_menu_t menu;
    app_menu_init(&menu, 16, 12);
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "a", "Alpha", "[USR]"));
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "b", "Beta", "[TOL]"));
    TEST_ASSERT_EQUAL(0, app_menu_add(&menu, "c", "Gamma", "[GME]"));
    TEST_ASSERT_EQUAL(3, menu.count);
    TEST_ASSERT_EQUAL(0, menu.selected);

    TEST_ASSERT_TRUE(app_menu_move(&menu, +1, 64));
    TEST_ASSERT_EQUAL(1, menu.selected);
    TEST_ASSERT_EQUAL_STRING("b", app_menu_selected(&menu)->id);

    TEST_ASSERT_TRUE(app_menu_move(&menu, -1, 64));
    TEST_ASSERT_EQUAL(0, menu.selected);
    TEST_ASSERT_FALSE(app_menu_move(&menu, -1, 64));
}

void test_app_menu_load_catalog_uses_boot_snapshot(void) {
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
    /* Second load must replace, not grow — catalog is the source of truth. */
    TEST_ASSERT_EQUAL(2, app_menu_load_catalog(&menu));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_app_kit_make_manifest_fills_fields);
    RUN_TEST(test_app_dirty_helpers);
    RUN_TEST(test_app_textf_and_flush_do_not_crash);
    RUN_TEST(test_app_request_exit_stops_running_flag);
    RUN_TEST(test_app_kit_is_foreground_false_without_focus);
    RUN_TEST(test_app_bind_key_succeeds_then_rejects_overflow);
    RUN_TEST(test_app_menu_move_and_select);
    RUN_TEST(test_app_menu_load_catalog_uses_boot_snapshot);
    return UNITY_END();
}
