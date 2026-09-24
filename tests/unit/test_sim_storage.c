#include "unity.h"
#include "sim_storage.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {
    sim_storage_init("test_flash.img", "test_sd.img");
}

void tearDown(void) {
    sim_storage_cleanup();
    remove("test_flash.img");
    remove("test_sd.img");
}

void test_storage_init_cleanup(void) {
    TEST_ASSERT_TRUE(sim_storage_flash_exists());
    TEST_ASSERT_TRUE(sim_storage_sd_exists());
    
    sim_storage_cleanup();
    
    TEST_ASSERT_FALSE(sim_storage_flash_exists());
    TEST_ASSERT_FALSE(sim_storage_sd_exists());
}

void test_flash_read_write(void) {
    uint8_t write_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t read_data[5] = {0};
    
    int ret = sim_storage_flash_write(0x1000, write_data, 5);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = sim_storage_flash_read(0x1000, read_data, 5);
    TEST_ASSERT_EQUAL(0, ret);
    
    TEST_ASSERT_EQUAL_UINT8_ARRAY(write_data, read_data, 5);
}

void test_flash_erase(void) {
    uint8_t write_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t read_data[4] = {0};
    
    sim_storage_flash_write(0x2000, write_data, 4);
    sim_storage_flash_read(0x2000, read_data, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(write_data, read_data, 4);
    
    sim_storage_flash_erase(0x2000, 4);
    sim_storage_flash_read(0x2000, read_data, 4);
    
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, read_data[i]);
    }
}

void test_sd_read_write(void) {
    uint8_t write_data[512];
    for (int i = 0; i < 512; i++) write_data[i] = i & 0xFF;
    
    uint8_t read_data[512] = {0};
    
    int ret = sim_storage_sd_write(0, write_data, 512);
    TEST_ASSERT_EQUAL(0, ret);
    
    ret = sim_storage_sd_read(0, read_data, 512);
    TEST_ASSERT_EQUAL(0, ret);
    
    TEST_ASSERT_EQUAL_UINT8_ARRAY(write_data, read_data, 512);
}

void test_storage_sizes(void) {
    TEST_ASSERT_EQUAL_UINT32(4 * 1024 * 1024, sim_storage_flash_size());
    TEST_ASSERT_EQUAL_UINT32(32 * 1024 * 1024, sim_storage_sd_size());
}

void test_storage_bounds(void) {
    uint8_t data[10];
    
    int ret = sim_storage_flash_read(0xFFFFFFF0, data, 10);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    ret = sim_storage_flash_write(0xFFFFFFF0, data, 10);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    ret = sim_storage_sd_read(0xFFFFFFFF, data, 10);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_storage_init_cleanup);
    RUN_TEST(test_flash_read_write);
    RUN_TEST(test_flash_erase);
    RUN_TEST(test_sd_read_write);
    RUN_TEST(test_storage_sizes);
    RUN_TEST(test_storage_bounds);
    
    return UNITY_END();
}