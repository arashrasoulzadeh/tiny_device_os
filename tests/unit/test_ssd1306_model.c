#include "unity.h"
#include "ssd1306_model.h"
#include "sim_i2c.h"
#include "sim_video.h"

void setUp(void) {
    sim_i2c_init();
    sim_video_init(128, 64, "Test");
    ssd1306_model_register();
}

void tearDown(void) {
    sim_video_cleanup();
    sim_i2c_cleanup();
}

void test_ssd1306_register(void) {
    uint8_t data[1];
    int ret = sim_i2c_read(SSD1306_I2C_ADDR, data, 1);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_ssd1306_write_command(void) {
    uint8_t cmd[] = {0x00, SSD1306_CMD_DISPLAY_OFF};
    int ret = sim_i2c_write(SSD1306_I2C_ADDR, cmd, 2);
    TEST_ASSERT_EQUAL(0, ret);
    
    cmd[1] = SSD1306_CMD_DISPLAY_ON;
    ret = sim_i2c_write(SSD1306_I2C_ADDR, cmd, 2);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_ssd1306_write_data(void) {
    uint8_t data[] = {0x40, 0xFF, 0xFF, 0xFF, 0xFF};
    int ret = sim_i2c_write(SSD1306_I2C_ADDR, data, 5);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_ssd1306_invert_display(void) {
    uint8_t cmd[] = {0x00, SSD1306_CMD_INVERT_DISPLAY};
    sim_i2c_write(SSD1306_I2C_ADDR, cmd, 2);
    
    cmd[1] = SSD1306_CMD_NORMAL_DISPLAY;
    sim_i2c_write(SSD1306_I2C_ADDR, cmd, 2);
}

void test_ssd1306_contrast(void) {
    uint8_t cmd[] = {0x00, SSD1306_CMD_SET_CONTRAST, 0x7F};
    int ret = sim_i2c_write(SSD1306_I2C_ADDR, cmd, 3);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_ssd1306_render(void) {
    uint8_t data[] = {0x40};
    for (int i = 0; i < 128; i++) {
        data[1] = 0xFF;
        sim_i2c_write(SSD1306_I2C_ADDR, data, 2);
    }
    
    ssd1306_model_render();
    
    uint32_t* pixels = sim_video_get_pixels();
    TEST_ASSERT_NOT_NULL(pixels);
    
    bool found_white = false;
    for (int i = 0; i < 128 * 64; i++) {
        if (pixels[i] == 0xFFFFFFFF) {
            found_white = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_white);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ssd1306_register);
    RUN_TEST(test_ssd1306_write_command);
    RUN_TEST(test_ssd1306_write_data);
    RUN_TEST(test_ssd1306_invert_display);
    RUN_TEST(test_ssd1306_contrast);
    RUN_TEST(test_ssd1306_render);
    
    return UNITY_END();
}