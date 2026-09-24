#include "unity.h"
#include "sim_i2c.h"
#include "sim_spi.h"

void setUp(void) {
    sim_i2c_init();
    sim_spi_init();
}

void tearDown(void) {
    sim_i2c_cleanup();
    sim_spi_cleanup();
}

static int i2c_dev1_read(uint8_t addr, uint16_t reg, uint8_t* data, size_t len, void* arg) {
    *(int*)arg += 1;
    return 0;
}

static int i2c_dev1_write(uint8_t addr, uint16_t reg, const uint8_t* data, size_t len, void* arg) {
    *(int*)arg += 1;
    return 0;
}

static int i2c_dev2_read(uint8_t addr, uint16_t reg, uint8_t* data, size_t len, void* arg) {
    *(int*)arg += 1;
    return 0;
}

static int spi_dev_transfer(uint8_t cs, const uint8_t* tx, uint8_t* rx, size_t len, void* arg) {
    *(int*)arg += 1;
    return 0;
}

void test_i2c_device_registry(void) {
    int read_count = 0;
    int write_count = 0;
    
    int ret = sim_i2c_register_device(0x48, i2c_dev1_read, i2c_dev1_write, &read_count);
    TEST_ASSERT_EQUAL(0, ret);
    
    uint8_t data[2];
    sim_i2c_read(0x48, data, 2);
    TEST_ASSERT_EQUAL(1, read_count);
    
    sim_i2c_write(0x48, data, 2);
    TEST_ASSERT_EQUAL(1, write_count);
    
    sim_i2c_unregister_device(0x48);
    
    sim_i2c_read(0x48, data, 2);
    TEST_ASSERT_EQUAL(1, read_count);
}

void test_spi_device_registry(void) {
    int transfer_count = 0;
    
    int ret = sim_spi_register_device(0, spi_dev_transfer, &transfer_count);
    TEST_ASSERT_EQUAL(0, ret);
    
    uint8_t tx[] = {0x01, 0x02};
    uint8_t rx[2];
    sim_spi_transfer(0, tx, rx, 2);
    TEST_ASSERT_EQUAL(1, transfer_count);
    
    sim_spi_unregister_device(0);
    
    sim_spi_transfer(0, tx, rx, 2);
    TEST_ASSERT_EQUAL(1, transfer_count);
}

void test_i2c_multiple_devices(void) {
    int dev1_reads = 0;
    int dev2_reads = 0;
    
    sim_i2c_register_device(0x48, i2c_dev1_read, NULL, &dev1_reads);
    
    sim_i2c_register_device(0x49, i2c_dev2_read, NULL, &dev2_reads);
    
    uint8_t data[1];
    sim_i2c_read(0x48, data, 1);
    sim_i2c_read(0x48, data, 1);
    sim_i2c_read(0x49, data, 1);
    
    TEST_ASSERT_EQUAL(2, dev1_reads);
    TEST_ASSERT_EQUAL(1, dev2_reads);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_i2c_device_registry);
    RUN_TEST(test_spi_device_registry);
    RUN_TEST(test_i2c_multiple_devices);
    
    return UNITY_END();
}