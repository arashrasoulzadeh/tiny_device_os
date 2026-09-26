#include "unity.h"
#include "app_kit.h"
#include "canvas.h"
#include "scheduler.h"
#include <string.h>

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_canvas_dirty_helpers(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    TEST_ASSERT_FALSE(app_is_dirty(&ctx));
    app_mark_dirty(&ctx);
    TEST_ASSERT_TRUE(app_is_dirty(&ctx));
    app_clear_dirty(&ctx);
    TEST_ASSERT_FALSE(app_is_dirty(&ctx));
}

void test_canvas_draw_helpers_safe_when_not_foreground(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    TEST_ASSERT_EQUAL(0, app_display_init(&ctx.display, "/dev/display0"));

    app_clear(&ctx);
    app_text(&ctx, 0, 0, "Hello");
    app_textf(&ctx, 0, 8, "n=%d", 42);
    app_flush(&ctx);
    app_mark_dirty(&ctx);
    TEST_ASSERT_TRUE(app_is_dirty(&ctx));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_canvas_dirty_helpers);
    RUN_TEST(test_canvas_draw_helpers_safe_when_not_foreground);
    return UNITY_END();
}
