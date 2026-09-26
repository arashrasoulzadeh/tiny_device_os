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

    sim_key_t keys[] = {SIM_KEY_3, SIM_KEY_4, SIM_KEY_5, SIM_KEY_6, SIM_KEY_7, SIM_KEY_8};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        TEST_ASSERT_EQUAL(0, app_bind_key(&ctx, keys[i], dummy_key, NULL));
    }
    TEST_ASSERT_EQUAL(APP_KIT_MAX_KEYS, (int)ctx.key_count);
    TEST_ASSERT_NOT_EQUAL(0, app_bind_key(&ctx, SIM_KEY_9, dummy_key, NULL));
}

void test_app_bind_back_registers_escape(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    TEST_ASSERT_EQUAL(0, app_bind_back(&ctx));
    TEST_ASSERT_EQUAL(1, (int)ctx.key_count);
    TEST_ASSERT_EQUAL(SIM_KEY_ESCAPE, ctx.keys[0].key);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_app_kit_make_manifest_fills_fields);
    RUN_TEST(test_app_request_exit_stops_running_flag);
    RUN_TEST(test_app_kit_is_foreground_false_without_focus);
    RUN_TEST(test_app_bind_key_succeeds_then_rejects_overflow);
    RUN_TEST(test_app_bind_back_registers_escape);
    return UNITY_END();
}
