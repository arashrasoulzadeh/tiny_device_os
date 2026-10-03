#include "unity.h"
#include "ui.h"
#include "app_framework.h"
#include "scheduler.h"
#include <string.h>

void setUp(void) {
    scheduler_init();
}

void tearDown(void) {}

void test_context_create_destroy(void) {
    ui_context_t* ctx = ui_context_create(100, 100, 100, 100);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_NOT_NULL(ctx->root);
    ui_context_destroy(ctx);
}

/* Regression test for a real heap-buffer-overflow that used to exist here:
 * ui_flex_create() allocated sizeof(ui_widget_t) bytes but wrote/read
 * ui_container_t fields (flex_dir/justify/align/gap) through the extra
 * bytes past the end of that allocation. Interleaving plain widget
 * allocations with container allocations is exactly the pattern that
 * would corrupt a later widget's fields if the overflow were still there -
 * this fails without the fix (sometimes silently, sometimes by crashing),
 * passes with it. */
void test_flex_container_allocation_is_not_corrupted(void) {
    ui_context_t* ctx = ui_context_create(100, 100, 100, 100);
    TEST_ASSERT_NOT_NULL(ctx);

    ui_container_t* a = ui_flex_create(ctx, UI_FLEX_DIR_ROW, UI_JUSTIFY_START, UI_ALIGN_START, 5);
    ui_widget_t* filler1 = ui_widget_create(ctx, UI_WIDGET_LABEL, "filler1");
    ui_container_t* b = ui_flex_create(ctx, UI_FLEX_DIR_COL, UI_JUSTIFY_CENTER, UI_ALIGN_CENTER, 9);
    ui_widget_t* filler2 = ui_widget_create(ctx, UI_WIDGET_LABEL, "filler2");

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(filler1);
    TEST_ASSERT_NOT_NULL(filler2);

    TEST_ASSERT_EQUAL(UI_FLEX_DIR_ROW, a->flex_dir);
    TEST_ASSERT_EQUAL(5, a->gap);
    TEST_ASSERT_EQUAL(UI_FLEX_DIR_COL, b->flex_dir);
    TEST_ASSERT_EQUAL(9, b->gap);
    TEST_ASSERT_EQUAL_STRING("filler1", filler1->name);
    TEST_ASSERT_EQUAL_STRING("filler2", filler2->name);

    ui_widget_destroy((ui_widget_t*)a);
    ui_widget_destroy((ui_widget_t*)b);
    ui_widget_destroy(filler1);
    ui_widget_destroy(filler2);
    ui_context_destroy(ctx);
}

void test_flex_row_layout_splits_width(void) {
    ui_context_t* ctx = ui_context_create(100, 50, 100, 50);
    TEST_ASSERT_NOT_NULL(ctx);

    ui_container_t* row = ui_flex_create(ctx, UI_FLEX_DIR_ROW, UI_JUSTIFY_START, UI_ALIGN_START, 0);
    ui_widget_set_rect((ui_widget_t*)row, 0, 0, 100, 50);
    ui_widget_set_padding((ui_widget_t*)row, 0, 0, 0, 0);  // default style padding is 8px/side
    ui_widget_add_child(ctx->root, (ui_widget_t*)row);

    ui_widget_t* left = ui_widget_create(ctx, UI_WIDGET_LABEL, "left");
    left->layout_params.width_mode = UI_SIZE_MODE_FLEX;
    left->layout_params.flex_grow = 1;
    ui_widget_add_child((ui_widget_t*)row, left);

    ui_widget_t* right = ui_widget_create(ctx, UI_WIDGET_LABEL, "right");
    right->layout_params.width_mode = UI_SIZE_MODE_FLEX;
    right->layout_params.flex_grow = 1;
    ui_widget_add_child((ui_widget_t*)row, right);

    ui_layout(ctx);

    TEST_ASSERT_EQUAL(50, left->rect.w);
    TEST_ASSERT_EQUAL(50, right->rect.w);
    TEST_ASSERT_EQUAL(50, right->rect.x);

    ui_context_destroy(ctx);
}

/* Proves the rendering pipeline is actually wired to the real display now,
 * not just the widget tree - draw_rect/draw_text used to be no-ops. */
void test_render_draws_to_real_display_without_crashing(void) {
    app_display_t display;
    TEST_ASSERT_EQUAL(0, app_display_init(&display, "/dev/display0"));

    ui_context_t* ctx = ui_context_create(100, 50, 100, 50);
    TEST_ASSERT_NOT_NULL(ctx);
    ui_context_set_display(ctx, &display);

    ui_widget_t* label = ui_label_create(ctx, "hello");
    ui_widget_set_rect(label, 0, 0, 40, 10);
    ui_widget_add_child(ctx->root, label);

    ui_widget_t* button = ui_button_create(ctx, "go", NULL);
    ui_widget_set_rect(button, 0, 20, 40, 16);
    button->focused = true;  // exercises the draw_rect(focus color) path too
    ui_widget_add_child(ctx->root, button);

    ui_render(ctx);
    app_display_flush(&display);

    ui_context_destroy(ctx);
    app_display_deinit(&display);
}

/* Regression test: ui_widget_focus() used to only ever set widget->focused
 * and never touch ctx->focused, so ui_widget_get_focused() always returned
 * NULL and Tab/Enter in ui_input_event() were both dead - Enter checked
 * ctx->focused (always NULL), and Tab's "next widget" logic didn't exist. */
static int g_click_count;
static void count_click(ui_widget_t* w, void* arg) { (void)w; (void)arg; g_click_count++; }

void test_focus_sets_ctx_focused_and_unfocuses_previous(void) {
    ui_context_t* ctx = ui_context_create(100, 100, 100, 100);
    ui_widget_t* a = ui_button_create(ctx, "a", count_click);
    ui_widget_t* b = ui_button_create(ctx, "b", count_click);

    ui_widget_focus(a);
    TEST_ASSERT_EQUAL_PTR(a, ui_widget_get_focused(ctx));
    TEST_ASSERT_TRUE(a->focused);

    ui_widget_focus(b);
    TEST_ASSERT_EQUAL_PTR(b, ui_widget_get_focused(ctx));
    TEST_ASSERT_FALSE(a->focused);  // focusing b must unfocus a
    TEST_ASSERT_TRUE(b->focused);

    ui_widget_destroy(a);
    ui_widget_destroy(b);
    ui_context_destroy(ctx);
}

void test_enter_key_clicks_the_focused_widget(void) {
    ui_context_t* ctx = ui_context_create(100, 100, 100, 100);
    ui_widget_t* button = ui_button_create(ctx, "go", count_click);
    ui_widget_focus(button);

    g_click_count = 0;
    input_event_t ev = {0};
    ev.type = INPUT_EVENT_KEY_DOWN;
    ev.key = INPUT_KEY_ENTER;
    ui_input_event(ctx, &ev);

    TEST_ASSERT_EQUAL(1, g_click_count);

    ui_widget_destroy(button);
    ui_context_destroy(ctx);
}

void test_tab_cycles_focus_through_focusable_widgets(void) {
    ui_context_t* ctx = ui_context_create(100, 100, 100, 100);
    ui_widget_t* a = ui_button_create(ctx, "a", count_click);
    ui_widget_t* b = ui_button_create(ctx, "b", count_click);
    ui_widget_add_child(ctx->root, a);
    ui_widget_add_child(ctx->root, b);
    ui_widget_focus(a);

    input_event_t tab = {0};
    tab.type = INPUT_EVENT_KEY_DOWN;
    tab.key = INPUT_KEY_TAB;

    ui_input_event(ctx, &tab);
    TEST_ASSERT_EQUAL_PTR(b, ui_widget_get_focused(ctx));

    ui_input_event(ctx, &tab);  // wraps back to a
    TEST_ASSERT_EQUAL_PTR(a, ui_widget_get_focused(ctx));

    ui_context_destroy(ctx);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_context_create_destroy);
    RUN_TEST(test_flex_container_allocation_is_not_corrupted);
    RUN_TEST(test_flex_row_layout_splits_width);
    RUN_TEST(test_focus_sets_ctx_focused_and_unfocuses_previous);
    RUN_TEST(test_enter_key_clicks_the_focused_widget);
    RUN_TEST(test_tab_cycles_focus_through_focusable_widgets);
    RUN_TEST(test_render_draws_to_real_display_without_crashing);
    return UNITY_END();
}
