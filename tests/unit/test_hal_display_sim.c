#include "unity.h"
#include "hal_display.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_display_open_close(void) {
    hal_display_config_t config = {
        .width = 128,
        .height = 64,
        .bpp = 1,
        .interface = HAL_DISPLAY_INTERFACE_I2C,
        .color_format = HAL_DISPLAY_COLOR_MONO
    };
    
    hal_display_t* display = hal_display_open("/dev/display0", &config);
    TEST_ASSERT_NOT_NULL(display);
    
    hal_display_close(display);
}

void test_display_init_deinit(void) {
    hal_display_config_t config = {
        .width = 128,
        .height = 64,
        .bpp = 1,
        .interface = HAL_DISPLAY_INTERFACE_I2C,
        .color_format = HAL_DISPLAY_COLOR_MONO
    };
    
    hal_display_t* display = hal_display_open("/dev/display0", &config);
    TEST_ASSERT_NOT_NULL(display);
    
    int ret = hal_display_init(display);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = hal_display_deinit(display);
    TEST_ASSERT_EQUAL(0, ret);
    
    hal_display_close(display);
}

void test_display_draw_operations(void) {
    hal_display_config_t config = {
        .width = 128,
        .height = 64,
        .bpp = 1,
        .interface = HAL_DISPLAY_INTERFACE_I2C,
        .color_format = HAL_DISPLAY_COLOR_MONO
    };
    
    hal_display_t* display = hal_display_open("/dev/display0", &config);
    TEST_ASSERT_NOT_NULL(display);
    
    hal_display_init(display);
    
    uint8_t bitmap[] = {0xFF, 0xFF, 0xFF, 0xFF};
    int ret = hal_display_draw_bitmap(display, 0, 0, 32, 8, bitmap);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = hal_display_fill_rect(display, 10, 10, 20, 20, 0xFFFFFF);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = hal_display_draw_pixel(display, 5, 5, 0xFFFFFF);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = hal_display_draw_line(display, 0, 0, 10, 10, 0xFFFFFF);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = hal_display_draw_rect(display, 20, 20, 30, 30, 0xFFFFFF);
    TEST_ASSERT_EQUAL(0, ret);
    
    hal_display_deinit(display);
    hal_display_close(display);
}

void test_display_rotation(void) {
    hal_display_config_t config = {
        .width = 128,
        .height = 64,
        .bpp = 1,
        .interface = HAL_DISPLAY_INTERFACE_I2C,
        .color_format = HAL_DISPLAY_COLOR_MONO
    };
    
    hal_display_t* display = hal_display_open("/dev/display0", &config);
    TEST_ASSERT_NOT_NULL(display);
    
    hal_display_set_rotation(display, HAL_DISPLAY_ROTATION_90);
    TEST_ASSERT_EQUAL(HAL_DISPLAY_ROTATION_90, hal_display_get_rotation(display));
    
    hal_display_set_rotation(display, HAL_DISPLAY_ROTATION_180);
    TEST_ASSERT_EQUAL(HAL_DISPLAY_ROTATION_180, hal_display_get_rotation(display));
    
    hal_display_close(display);
}

void test_display_brightness(void) {
    hal_display_config_t config = {
        .width = 128,
        .height = 64,
        .bpp = 1,
        .interface = HAL_DISPLAY_INTERFACE_I2C,
        .color_format = HAL_DISPLAY_COLOR_MONO
    };
    
    hal_display_t* display = hal_display_open("/dev/display0", &config);
    TEST_ASSERT_NOT_NULL(display);
    
    hal_display_set_brightness(display, 128);
    TEST_ASSERT_EQUAL(128, hal_display_get_brightness(display));
    
    hal_display_set_brightness(display, 255);
    TEST_ASSERT_EQUAL(255, hal_display_get_brightness(display));
    
    hal_display_close(display);
}

void test_display_sleep_wake(void) {
    hal_display_config_t config = {
        .width = 128,
        .height = 64,
        .bpp = 1,
        .interface = HAL_DISPLAY_INTERFACE_I2C,
        .color_format = HAL_DISPLAY_COLOR_MONO
    };
    
    hal_display_t* display = hal_display_open("/dev/display0", &config);
    TEST_ASSERT_NOT_NULL(display);
    
    int ret = hal_display_sleep(display);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = hal_display_wake(display);
    TEST_ASSERT_EQUAL(0, ret);
    
    hal_display_close(display);
}

void test_display_size(void) {
    hal_display_config_t config = {
        .width = 320,
        .height = 240,
        .bpp = 16,
        .interface = HAL_DISPLAY_INTERFACE_SPI,
        .color_format = HAL_DISPLAY_COLOR_RGB565
    };
    
    hal_display_t* display = hal_display_open("/dev/display0", &config);
    TEST_ASSERT_NOT_NULL(display);
    
    uint16_t w, h;
    hal_display_get_size(display, &w, &h);
    TEST_ASSERT_EQUAL(320, w);
    TEST_ASSERT_EQUAL(240, h);
    
    hal_display_close(display);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_display_open_close);
    RUN_TEST(test_display_init_deinit);
    RUN_TEST(test_display_draw_operations);
    RUN_TEST(test_display_rotation);
    RUN_TEST(test_display_brightness);
    RUN_TEST(test_display_sleep_wake);
    RUN_TEST(test_display_size);
    
    return UNITY_END();
}