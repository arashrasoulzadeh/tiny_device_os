#include "unity.h"
#include "ssd1306_model.h"
#include "sim_i2c.h"
#include "sim_video.h"

void setUp(void) {
    sim_i2c_init();
    sim_video_init(SSD1306_WIDTH, SSD1306_HEIGHT, "Test");
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
    uint8_t data[] = {0x40, 0xFF};
    for (int i = 0; i < SSD1306_WIDTH; i++) {
        sim_i2c_write(SSD1306_I2C_ADDR, data, 2);
    }

    ssd1306_model_render();

    uint32_t* pixels = sim_video_get_pixels();
    if (!pixels) {
        TEST_IGNORE_MESSAGE("SDL video unavailable (headless / no display)");
        return;
    }

    bool found_white = false;
    for (int i = 0; i < SSD1306_WIDTH * SSD1306_HEIGHT; i++) {
        if (pixels[i] == 0xFFFFFFFF) {
            found_white = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_white);
}

/* Regression: g_ssd1306.column was uint8_t, which can't represent
 * SSD1306_WIDTH for sim builds (320) - `column >= SSD1306_WIDTH` was
 * always false, so column silently wrapped via plain uint8_t overflow
 * at 256 instead of the intended explicit wrap-and-advance-page logic,
 * and page never advanced past 0. test_ssd1306_render above didn't
 * catch this - it only checks "some pixel is lit anywhere," which stays
 * true even with every column after 255 overwriting columns 0-63
 * instead of landing at their real position. This writes a single lit
 * byte at a specific far column (300, past the old 256-wrap point) and
 * checks that exact pixel, not just "something is lit somewhere." */
#if SSD1306_WIDTH > 256
void test_ssd1306_write_data_past_256_columns_lands_at_the_right_column(void) {
    uint8_t zero[] = {0x40, 0x00};
    uint8_t one[] = {0x40, 0x01};  // bit 0 set -> lights the top row of this byte's page

    for (int col = 0; col < 300; col++) {
        sim_i2c_write(SSD1306_I2C_ADDR, zero, 2);
    }
    sim_i2c_write(SSD1306_I2C_ADDR, one, 2);  // column 300

    ssd1306_model_render();

    uint32_t* pixels = sim_video_get_pixels();
    if (!pixels) {
        TEST_IGNORE_MESSAGE("SDL video unavailable (headless / no display)");
        return;
    }

    // Page 0, column 300, bit 0 -> pixel (x=300, y=0).
    int idx = 0 * SSD1306_WIDTH + 300;
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFF, pixels[idx]);

    // Column 0 must NOT also be lit - the old bug re-wrote it when
    // column wrapped at 256 instead of advancing past it.
    TEST_ASSERT_EQUAL_UINT32(0xFF000000, pixels[0]);
}
#endif

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ssd1306_register);
    RUN_TEST(test_ssd1306_write_command);
    RUN_TEST(test_ssd1306_write_data);
#if SSD1306_WIDTH > 256
    RUN_TEST(test_ssd1306_write_data_past_256_columns_lands_at_the_right_column);
#endif
    RUN_TEST(test_ssd1306_invert_display);
    RUN_TEST(test_ssd1306_contrast);
    RUN_TEST(test_ssd1306_render);
    
    return UNITY_END();
}