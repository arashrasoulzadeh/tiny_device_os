#include "unity.h"
#include "display_fb.h"

#include <string.h>

static uint16_t g_px[8 * 4];
static display_fb_t g_fb;

void setUp(void) {
    memset(g_px, 0, sizeof(g_px));
    display_fb_init(&g_fb, g_px, 8, 4);
}

void tearDown(void) {}

void test_fill_rect_writes_pixels_and_marks_dirty_rows(void) {
    int16_t y = -1;
    int16_t rows = -1;

    display_fb_fill_rect(&g_fb, 2, 1, 3, 2, 0xF800);

    TEST_ASSERT_EQUAL_HEX16(0xF800, g_px[1 * 8 + 2]);
    TEST_ASSERT_EQUAL_HEX16(0xF800, g_px[2 * 8 + 4]);
    TEST_ASSERT_EQUAL_HEX16(0x0000, g_px[1 * 8 + 1]);
    TEST_ASSERT_EQUAL_HEX16(0x0000, g_px[0 * 8 + 2]);
    TEST_ASSERT_TRUE(display_fb_dirty_rows(&g_fb, &y, &rows));
    TEST_ASSERT_EQUAL_INT16(1, y);
    TEST_ASSERT_EQUAL_INT16(2, rows);
}

void test_fill_rect_clips_and_merges_dirty_rows(void) {
    int16_t y = 0;
    int16_t rows = 0;

    display_fb_fill_rect(&g_fb, -2, -1, 4, 2, 0x001F);
    display_fb_fill_rect(&g_fb, 6, 3, 10, 10, 0x07E0);

    TEST_ASSERT_EQUAL_HEX16(0x001F, g_px[0]);
    TEST_ASSERT_EQUAL_HEX16(0x0000, g_px[2]);
    TEST_ASSERT_EQUAL_HEX16(0x07E0, g_px[3 * 8 + 6]);
    TEST_ASSERT_EQUAL_HEX16(0x0000, g_px[3 * 8 + 5]);
    TEST_ASSERT_TRUE(display_fb_dirty_rows(&g_fb, &y, &rows));
    TEST_ASSERT_EQUAL_INT16(0, y);
    TEST_ASSERT_EQUAL_INT16(4, rows);
}

void test_clear_dirty_makes_the_next_flush_a_noop(void) {
    int16_t y = 0;
    int16_t rows = 0;

    display_fb_fill_rect(&g_fb, 0, 0, 1, 1, 0xFFFF);
    display_fb_clear_dirty(&g_fb);

    TEST_ASSERT_FALSE(display_fb_dirty_rows(&g_fb, &y, &rows));
}

void test_draw_pixel_and_horizontal_line(void) {
    display_fb_draw_pixel(&g_fb, 3, 2, 0x1111);
    display_fb_draw_line(&g_fb, 0, 3, 3, 3, 0x2222);

    TEST_ASSERT_EQUAL_HEX16(0x1111, g_px[2 * 8 + 3]);
    TEST_ASSERT_EQUAL_HEX16(0x2222, g_px[3 * 8 + 0]);
    TEST_ASSERT_EQUAL_HEX16(0x2222, g_px[3 * 8 + 3]);
    TEST_ASSERT_EQUAL_HEX16(0x0000, g_px[3 * 8 + 4]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fill_rect_writes_pixels_and_marks_dirty_rows);
    RUN_TEST(test_fill_rect_clips_and_merges_dirty_rows);
    RUN_TEST(test_clear_dirty_makes_the_next_flush_a_noop);
    RUN_TEST(test_draw_pixel_and_horizontal_line);
    return UNITY_END();
}
