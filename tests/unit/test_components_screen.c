#include "unity.h"
#include "app_kit.h"
#include "screen.h"
#include "scheduler.h"
#include <string.h>

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_screen_begin_false_when_clean(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    TEST_ASSERT_FALSE(app_screen_begin(&ctx, "Title"));
}

void test_screen_begin_true_when_dirty(void) {
    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    TEST_ASSERT_EQUAL(0, app_display_init(&ctx.display, "/dev/display0"));
    app_mark_dirty(&ctx);
    /* Not foreground: still reports dirty and "begins", draw is gated by canvas. */
    TEST_ASSERT_TRUE(app_screen_begin(&ctx, "Title"));
    app_screen_end(&ctx);
    TEST_ASSERT_FALSE(app_is_dirty(&ctx));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_screen_begin_false_when_clean);
    RUN_TEST(test_screen_begin_true_when_dirty);
    return UNITY_END();
}
