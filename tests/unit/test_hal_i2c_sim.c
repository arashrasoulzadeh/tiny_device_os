#include "unity.h"
#include "hal_i2c.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_i2c_open_close(void) {
    hal_i2c_t* i2c = hal_i2c_open("/dev/i2c0", HAL_I2C_SPEED_STANDARD);
    TEST_ASSERT_NOT_NULL(i2c);
    
    hal_i2c_close(i2c);
}

void test_i2c_write_read(void) {
    hal_i2c_t* i2c = hal_i2c_open("/dev/i2c0", HAL_I2C_SPEED_STANDARD);
    TEST_ASSERT_NOT_NULL(i2c);
    
    uint8_t write_data[] = {0x01, 0x02, 0x03};
    int ret = hal_i2c_write(i2c, 0x48, write_data, 3);
    TEST_ASSERT_EQUAL(0, ret);
    
    uint8_t read_data[3] = {0};
    ret = hal_i2c_read(i2c, 0x48, read_data, 3);
    TEST_ASSERT_EQUAL(0, ret);
    
    hal_i2c_close(i2c);
}

void test_i2c_write_read_combined(void) {
    hal_i2c_t* i2c = hal_i2c_open("/dev/i2c0", HAL_I2C_SPEED_STANDARD);
    TEST_ASSERT_NOT_NULL(i2c);
    
    uint8_t write_data[] = {0x00};
    uint8_t read_data[2] = {0};
    
    int ret = hal_i2c_write_read(i2c, 0x48, write_data, 1, read_data, 2);
    TEST_ASSERT_EQUAL(0, ret);
    
    hal_i2c_close(i2c);
}

void test_i2c_mem_operations(void) {
    hal_i2c_t* i2c = hal_i2c_open("/dev/i2c0", HAL_I2C_SPEED_STANDARD);
    TEST_ASSERT_NOT_NULL(i2c);
    
    uint8_t write_data[] = {0xAA, 0xBB};
    int ret = hal_i2c_mem_write(i2c, 0x50, 0x0010, 2, write_data, 2);
    TEST_ASSERT_EQUAL(0, ret);
    
    uint8_t read_data[2] = {0};
    ret = hal_i2c_mem_read(i2c, 0x50, 0x0010, 2, read_data, 2);
    TEST_ASSERT_EQUAL(0, ret);
    
    hal_i2c_close(i2c);
}

void test_i2c_scan(void) {
    hal_i2c_t* i2c = hal_i2c_open("/dev/i2c0", HAL_I2C_SPEED_STANDARD);
    TEST_ASSERT_NOT_NULL(i2c);
    
    uint8_t addrs[16];
    size_t found = 0;
    int ret = hal_i2c_scan(i2c, addrs, 16, &found);
    TEST_ASSERT_EQUAL(0, ret);
    
    hal_i2c_close(i2c);
}

void test_i2c_timeout(void) {
    hal_i2c_t* i2c = hal_i2c_open("/dev/i2c0", HAL_I2C_SPEED_STANDARD);
    TEST_ASSERT_NOT_NULL(i2c);
    
    hal_i2c_set_timeout(i2c, 500);
    TEST_ASSERT_EQUAL(500, hal_i2c_get_timeout(i2c));
    
    hal_i2c_close(i2c);
}

void test_i2c_speeds(void) {
    hal_i2c_t* i2c1 = hal_i2c_open("/dev/i2c0", HAL_I2C_SPEED_STANDARD);
    TEST_ASSERT_NOT_NULL(i2c1);
    
    hal_i2c_t* i2c2 = hal_i2c_open("/dev/i2c1", HAL_I2C_SPEED_FAST);
    TEST_ASSERT_NOT_NULL(i2c2);
    
    hal_i2c_t* i2c3 = hal_i2c_open("/dev/i2c2", HAL_I2C_SPEED_FAST_PLUS);
    TEST_ASSERT_NOT_NULL(i2c3);
    
    hal_i2c_t* i2c4 = hal_i2c_open("/dev/i2c3", HAL_I2C_SPEED_HIGH);
    TEST_ASSERT_NOT_NULL(i2c4);
    
    hal_i2c_close(i2c1);
    hal_i2c_close(i2c2);
    hal_i2c_close(i2c3);
    hal_i2c_close(i2c4);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_i2c_open_close);
    RUN_TEST(test_i2c_write_read);
    RUN_TEST(test_i2c_write_read_combined);
    RUN_TEST(test_i2c_mem_operations);
    RUN_TEST(test_i2c_scan);
    RUN_TEST(test_i2c_timeout);
    RUN_TEST(test_i2c_speeds);
    
    return UNITY_END();
}