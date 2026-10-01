#include "unity.h"
#include "app_framework.h"
#include "app_ui.h"
#include "scheduler.h"
#include <string.h>

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_app_ui_init_links_desc_from_real_app(void) {
    app_desc_t desc = {0};
    desc.name = "launcher";

    app_ctx_t real_ctx;
    memset(&real_ctx, 0, sizeof(real_ctx));
    real_ctx.desc = &desc;

    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TEST", "help");

    app_ui_t ui;
    TEST_ASSERT_EQUAL(0, app_ui_init(&ui, &real_ctx, &cfg));

    /* Regression: without linking desc, app_open()/app_request_exit() on
     * ui.ctx silently no-op because they bail out on a NULL desc - this is
     * why Enter did nothing in the launcher despite arrow keys working. */
    TEST_ASSERT_EQUAL_PTR(&desc, ui.ctx.desc);
    TEST_ASSERT_EQUAL_STRING("launcher", ui.ctx.desc->name);

    app_ui_deinit(&ui);
}

void test_app_ui_init_tolerates_null_real_app(void) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "TEST", "help");

    app_ui_t ui;
    TEST_ASSERT_EQUAL(0, app_ui_init(&ui, NULL, &cfg));
    TEST_ASSERT_NULL(ui.ctx.desc);

    app_ui_deinit(&ui);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_app_ui_init_links_desc_from_real_app);
    RUN_TEST(test_app_ui_init_tolerates_null_real_app);
    return UNITY_END();
}
