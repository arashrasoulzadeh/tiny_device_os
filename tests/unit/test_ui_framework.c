#include "app_framework.h"
#include "fw/ui.h"
#include "scheduler.h"
#include "ui.h"
#include "unity.h"
#include <string.h>

void setUp(void) { scheduler_init(); }

void tearDown(void) {}

void test_context_create_destroy(void) {
  ui_context_t *ctx = ui_context_create(100, 100, 100, 100);
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
  ui_context_t *ctx = ui_context_create(100, 100, 100, 100);
  TEST_ASSERT_NOT_NULL(ctx);

  ui_container_t *a =
      ui_flex_create(ctx, UI_FLEX_DIR_ROW, UI_JUSTIFY_START, UI_ALIGN_START, 5);
  ui_widget_t *filler1 = ui_widget_create(ctx, UI_WIDGET_LABEL, "filler1");
  ui_container_t *b = ui_flex_create(ctx, UI_FLEX_DIR_COL, UI_JUSTIFY_CENTER,
                                     UI_ALIGN_CENTER, 9);
  ui_widget_t *filler2 = ui_widget_create(ctx, UI_WIDGET_LABEL, "filler2");

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

  ui_widget_destroy((ui_widget_t *)a);
  ui_widget_destroy((ui_widget_t *)b);
  ui_widget_destroy(filler1);
  ui_widget_destroy(filler2);
  ui_context_destroy(ctx);
}

void test_flex_row_layout_splits_width(void) {
  ui_context_t *ctx = ui_context_create(100, 50, 100, 50);
  TEST_ASSERT_NOT_NULL(ctx);

  ui_container_t *row =
      ui_flex_create(ctx, UI_FLEX_DIR_ROW, UI_JUSTIFY_START, UI_ALIGN_START, 0);
  ui_widget_set_rect((ui_widget_t *)row, 0, 0, 100, 50);
  ui_widget_set_padding((ui_widget_t *)row, 0, 0, 0,
                        0); // default style padding is 8px/side
  ui_widget_add_child(ctx->root, (ui_widget_t *)row);

  ui_widget_t *left = ui_widget_create(ctx, UI_WIDGET_LABEL, "left");
  left->layout_params.width_mode = UI_SIZE_MODE_FLEX;
  left->layout_params.flex_grow = 1;
  ui_widget_add_child((ui_widget_t *)row, left);

  ui_widget_t *right = ui_widget_create(ctx, UI_WIDGET_LABEL, "right");
  right->layout_params.width_mode = UI_SIZE_MODE_FLEX;
  right->layout_params.flex_grow = 1;
  ui_widget_add_child((ui_widget_t *)row, right);

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

  ui_context_t *ctx = ui_context_create(100, 50, 100, 50);
  TEST_ASSERT_NOT_NULL(ctx);
  ui_context_set_display(ctx, &display);

  ui_widget_t *label = ui_label_create(ctx, "hello");
  ui_widget_set_rect(label, 0, 0, 40, 10);
  ui_widget_add_child(ctx->root, label);

  ui_widget_t *button = ui_button_create(ctx, "go", NULL);
  ui_widget_set_rect(button, 0, 20, 40, 16);
  button->focused = true; // exercises the draw_rect(focus color) path too
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
static void count_click(ui_widget_t *w, void *arg) {
  (void)w;
  (void)arg;
  g_click_count++;
}

void test_focus_sets_ctx_focused_and_unfocuses_previous(void) {
  ui_context_t *ctx = ui_context_create(100, 100, 100, 100);
  ui_widget_t *a = ui_button_create(ctx, "a", count_click);
  ui_widget_t *b = ui_button_create(ctx, "b", count_click);

  ui_widget_focus(a);
  TEST_ASSERT_EQUAL_PTR(a, ui_widget_get_focused(ctx));
  TEST_ASSERT_TRUE(a->focused);

  ui_widget_focus(b);
  TEST_ASSERT_EQUAL_PTR(b, ui_widget_get_focused(ctx));
  TEST_ASSERT_FALSE(a->focused); // focusing b must unfocus a
  TEST_ASSERT_TRUE(b->focused);

  ui_widget_destroy(a);
  ui_widget_destroy(b);
  ui_context_destroy(ctx);
}

void test_enter_key_clicks_the_focused_widget(void) {
  ui_context_t *ctx = ui_context_create(100, 100, 100, 100);
  ui_widget_t *button = ui_button_create(ctx, "go", count_click);
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
  ui_context_t *ctx = ui_context_create(100, 100, 100, 100);
  ui_widget_t *a = ui_button_create(ctx, "a", count_click);
  ui_widget_t *b = ui_button_create(ctx, "b", count_click);
  ui_widget_add_child(ctx->root, a);
  ui_widget_add_child(ctx->root, b);
  ui_widget_focus(a);

  input_event_t tab = {0};
  tab.type = INPUT_EVENT_KEY_DOWN;
  tab.key = INPUT_KEY_TAB;

  ui_input_event(ctx, &tab);
  TEST_ASSERT_EQUAL_PTR(b, ui_widget_get_focused(ctx));

  ui_input_event(ctx, &tab); // wraps back to a
  TEST_ASSERT_EQUAL_PTR(a, ui_widget_get_focused(ctx));

  ui_context_destroy(ctx);
}

/* Regression test: ui_grid_create() used to just alias a flex row, which
 * produces one row no matter how many columns are requested. A 2x2 grid
 * of 4 children should actually wrap into two rows. */
void test_grid_wraps_children_into_rows(void) {
  ui_context_t *ctx = ui_context_create(100, 100, 100, 100);
  ui_container_t *grid = ui_grid_create(ctx, 2, 2, 0);
  ui_widget_set_rect((ui_widget_t *)grid, 0, 0, 100, 100);
  ui_widget_set_padding((ui_widget_t *)grid, 0, 0, 0, 0);
  ui_widget_add_child(ctx->root, (ui_widget_t *)grid);

  ui_widget_t *cells[4];
  for (int i = 0; i < 4; i++) {
    cells[i] = ui_widget_create(ctx, UI_WIDGET_LABEL, "cell");
    ui_widget_add_child((ui_widget_t *)grid, cells[i]);
  }

  ui_layout(ctx);

  // 2 cols x 2 rows over a 100x100 area -> each cell 50x50.
  TEST_ASSERT_EQUAL(50, cells[0]->rect.w);
  TEST_ASSERT_EQUAL(50, cells[0]->rect.h);
  TEST_ASSERT_EQUAL(0, cells[0]->rect.x);
  TEST_ASSERT_EQUAL(0, cells[0]->rect.y);

  TEST_ASSERT_EQUAL(50, cells[1]->rect.x); // col 1, row 0
  TEST_ASSERT_EQUAL(0, cells[1]->rect.y);

  TEST_ASSERT_EQUAL(0, cells[2]->rect.x); // col 0, row 1 - the actual wrap
  TEST_ASSERT_EQUAL(50, cells[2]->rect.y);

  TEST_ASSERT_EQUAL(50, cells[3]->rect.x); // col 1, row 1
  TEST_ASSERT_EQUAL(50, cells[3]->rect.y);

  ui_context_destroy(ctx);
}

void test_grid_auto_computes_rows_from_child_count(void) {
  ui_context_t *ctx = ui_context_create(90, 60, 90, 60);
  ui_container_t *grid = ui_grid_create(ctx, 3, 0, 0); // 0 rows = auto
  ui_widget_set_rect((ui_widget_t *)grid, 0, 0, 90, 60);
  ui_widget_set_padding((ui_widget_t *)grid, 0, 0, 0, 0);
  ui_widget_add_child(ctx->root, (ui_widget_t *)grid);

  ui_widget_t *cells[3];
  for (int i = 0; i < 3; i++) {
    cells[i] = ui_widget_create(ctx, UI_WIDGET_LABEL, "cell");
    ui_widget_add_child((ui_widget_t *)grid, cells[i]);
  }

  ui_layout(ctx);

  // 3 children, 3 cols -> auto rows = 1, so full height per cell.
  TEST_ASSERT_EQUAL(60, cells[0]->rect.h);
  TEST_ASSERT_EQUAL(30, cells[0]->rect.w); // 90 / 3 cols

  ui_context_destroy(ctx);
}

/* Regression test: ui_scroll_create() used to just alias a flex column
 * with no offset or culling at all - every child always rendered at its
 * natural position regardless of any "scroll" state, because there was
 * no scroll state. */
void test_scroll_offset_shifts_children_and_culls_out_of_view(void) {
  ui_context_t *ctx = ui_context_create(50, 30, 50, 30);
  ui_container_t *scroll = ui_scroll_create(ctx, UI_FLEX_DIR_COL);
  ui_widget_set_rect((ui_widget_t *)scroll, 0, 0, 50, 30);
  ui_widget_set_padding((ui_widget_t *)scroll, 0, 0, 0, 0);
  ui_widget_add_child(ctx->root, (ui_widget_t *)scroll);

  // 3 rows of height 20 each inside a 30px-tall viewport - taller than
  // the viewport on purpose, to actually exercise overflow/culling.
  ui_widget_t *rows[3];
  for (int i = 0; i < 3; i++) {
    rows[i] = ui_widget_create(ctx, UI_WIDGET_LABEL, "row");
    rows[i]->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    rows[i]->layout_params.height = 20;
    ui_widget_add_child((ui_widget_t *)scroll, rows[i]);
  }

  ui_layout(ctx);
  TEST_ASSERT_EQUAL(0, rows[0]->rect.y);
  TEST_ASSERT_EQUAL(20, rows[1]->rect.y);
  TEST_ASSERT_TRUE(rows[0]->visible);
  TEST_ASSERT_TRUE(
      rows[1]->visible); // partially in view (y=20..40, viewport 0..30)
  TEST_ASSERT_FALSE(
      rows[2]->visible); // y=40..60, fully below the 30px viewport

  ui_scroll_set_offset(scroll, 20);
  ui_layout(ctx);
  // rect.y is unsigned - a scrolled-above position clamps to 0 rather
  // than going negative; row 0 is !visible anyway so its clamped
  // position never gets drawn.
  TEST_ASSERT_EQUAL(0, rows[0]->rect.y);
  TEST_ASSERT_EQUAL(0, rows[1]->rect.y);
  TEST_ASSERT_EQUAL(20, rows[2]->rect.y);
  TEST_ASSERT_FALSE(rows[0]->visible); // scrolled fully above the viewport now
  TEST_ASSERT_TRUE(rows[1]->visible);
  TEST_ASSERT_TRUE(rows[2]->visible);

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
  RUN_TEST(test_grid_wraps_children_into_rows);
  RUN_TEST(test_grid_auto_computes_rows_from_child_count);
  RUN_TEST(test_scroll_offset_shifts_children_and_culls_out_of_view);
  RUN_TEST(test_render_draws_to_real_display_without_crashing);
  return UNITY_END();
}
