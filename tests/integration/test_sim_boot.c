#include "unity.h"
#include "scheduler.h"
#include "os_time.h"
#include "sim_video.h"
#include "sim_storage.h"
#include "sim_time.h"
#include "sim_i2c.h"
#include "ssd1306_model.h"
#include "bmp280_model.h"
#include "sim_main.h"

extern bool g_running;
extern bool g_headless;

void setUp(void) {
    g_running = true;
    g_headless = true;
}

void tearDown(void) {
    sim_storage_cleanup();
    sim_video_cleanup();
}

void test_sim_boot_sequence(void) {
    const char* argv[] = {"ardubot-sim", "--headless", "--flash-image=test_flash.img", "--sd-image=test_sd.img"};
    parse_args(4, (char**)argv);
    
    TEST_ASSERT_EQUAL(0, scheduler_init());
    TEST_ASSERT_EQUAL(0, sim_time_init());
    TEST_ASSERT_EQUAL(0, sim_storage_init("test_flash.img", "test_sd.img"));
    TEST_ASSERT_EQUAL(0, sim_video_init(320, 240, "Test"));
    
    ssd1306_model_register();
    bmp280_model_register();
    
    TEST_ASSERT_TRUE(sim_storage_flash_exists());
    TEST_ASSERT_TRUE(sim_storage_sd_exists());
    
    TEST_ASSERT_EQUAL(0, scheduler_start());
    
    sim_time_update();
    sim_video_poll_events();
    
    if (!g_headless) {
        sim_video_render();
    }
    
    scheduler_tick();
    timers_process();
    
    g_running = false;
}

void test_device_models_registered(void) {
    sim_i2c_init();
    ssd1306_model_register();
    bmp280_model_register();
    
    uint8_t id = 0;
    sim_i2c_read(SSD1306_I2C_ADDR, &id, 1);
    
    id = 0;
    sim_i2c_read(BMP280_I2C_ADDR, &id, 1);
    TEST_ASSERT_EQUAL_HEX8(BMP280_CHIP_ID, id);
    
    sim_i2c_cleanup();
}

void test_storage_persistence(void) {
    sim_storage_init("persist_flash.img", "persist_sd.img");
    
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    sim_storage_flash_write(0x1000, data, 4);
    sim_storage_sd_write(0x2000, data, 4);
    
    sim_storage_cleanup();
    
    sim_storage_init("persist_flash.img", "persist_sd.img");
    
    uint8_t read_data[4];
    sim_storage_flash_read(0x1000, read_data, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, read_data, 4);
    
    sim_storage_sd_read(0x2000, read_data, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, read_data, 4);
    
    sim_storage_cleanup();
    remove("persist_flash.img");
    remove("persist_sd.img");
}

void test_main_loop_iteration(void) {
    scheduler_init();
    sim_time_init();
    sim_storage_init("loop_flash.img", "loop_sd.img");
    sim_video_init(320, 240, "Test");
    
    ssd1306_model_register();
    bmp280_model_register();
    
    scheduler_start();
    
    for (int i = 0; i < 10; i++) {
        sim_time_update();
        sim_video_poll_events();
        scheduler_tick();
        timers_process();
    }
    
    g_running = false;
    
    sim_storage_cleanup();
    sim_video_cleanup();
    remove("loop_flash.img");
    remove("loop_sd.img");
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_sim_boot_sequence);
    RUN_TEST(test_device_models_registered);
    RUN_TEST(test_storage_persistence);
    RUN_TEST(test_main_loop_iteration);
    
    return UNITY_END();
}