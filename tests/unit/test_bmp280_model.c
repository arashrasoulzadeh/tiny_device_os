#include "unity.h"
#include "bmp280_model.h"
#include "sim_i2c.h"

void setUp(void) {
    sim_i2c_init();
    bmp280_model_register();
}

void tearDown(void) {
    sim_i2c_cleanup();
}

void test_bmp280_chip_id(void) {
    uint8_t id = 0;
    int ret = sim_i2c_read(BMP280_I2C_ADDR, &id, 1);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL_HEX8(BMP280_CHIP_ID, id);
}

void test_bmp280_reset(void) {
    uint8_t cmd[] = {BMP280_REG_RESET, 0xB6};
    int ret = sim_i2c_write(BMP280_I2C_ADDR, cmd, 2);
    TEST_ASSERT_EQUAL(0, ret);
    
    TEST_ASSERT_EQUAL_FLOAT(25.0f, bmp280_model_get_temperature());
    TEST_ASSERT_EQUAL_FLOAT(1013.25f, bmp280_model_get_pressure());
}

void test_bmp280_read_temperature(void) {
    bmp280_model_set_temperature(23.5f);
    
    uint8_t reg = BMP280_REG_TEMP_MSB;
    uint8_t data[3];
    int ret = sim_i2c_write_read(BMP280_I2C_ADDR, &reg, 1, data, 3);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_bmp280_read_pressure(void) {
    bmp280_model_set_pressure(1000.0f);
    
    uint8_t reg = BMP280_REG_PRESS_MSB;
    uint8_t data[3];
    int ret = sim_i2c_write_read(BMP280_I2C_ADDR, &reg, 1, data, 3);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_bmp280_set_get_values(void) {
    bmp280_model_set_temperature(-10.0f);
    TEST_ASSERT_EQUAL_FLOAT(-10.0f, bmp280_model_get_temperature());
    
    bmp280_model_set_temperature(50.0f);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, bmp280_model_get_temperature());
    
    bmp280_model_set_pressure(900.0f);
    TEST_ASSERT_EQUAL_FLOAT(900.0f, bmp280_model_get_pressure());
    
    bmp280_model_set_pressure(1100.0f);
    TEST_ASSERT_EQUAL_FLOAT(1100.0f, bmp280_model_get_pressure());
}

void test_bmp280_calibration_registers(void) {
    uint8_t reg = BMP280_REG_CALIB_00;
    uint8_t data[24];
    int ret = sim_i2c_write_read(BMP280_I2C_ADDR, &reg, 1, data, 24);
    TEST_ASSERT_EQUAL(0, ret);
    
    TEST_ASSERT_EQUAL_HEX8(0x00, data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x6B, data[1]);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_bmp280_chip_id);
    RUN_TEST(test_bmp280_reset);
    RUN_TEST(test_bmp280_read_temperature);
    RUN_TEST(test_bmp280_read_pressure);
    RUN_TEST(test_bmp280_set_get_values);
    RUN_TEST(test_bmp280_calibration_registers);
    
    return UNITY_END();
}