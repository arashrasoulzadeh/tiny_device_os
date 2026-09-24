#include "unity.h"
#include "sim_gpio.h"
#include "sim_video.h"

void setUp(void) {
    sim_gpio_init();
    sim_video_init(320, 240, "Test");
}

void tearDown(void) {
    sim_video_cleanup();
    sim_gpio_cleanup();
}

void test_gpio_key_mapping(void) {
    sim_gpio_register(0, false);
    sim_gpio_set_key_mapping(SIM_KEY_UP, 0, true);
    
    sim_gpio_handle_key(SIM_KEY_UP, true);
    TEST_ASSERT_TRUE(sim_gpio_read(0));
    
    sim_gpio_handle_key(SIM_KEY_UP, false);
    TEST_ASSERT_FALSE(sim_gpio_read(0));
}

static void gpio_key_callback(int pin, bool level, void* arg) {
    *(int*)arg = pin;
    *((bool*)arg + 1) = level;
}

void test_gpio_key_callback(void) {
    int cb_pin = -1;
    bool cb_level = false;
    
    sim_gpio_register(1, false);
    sim_gpio_set_key_mapping(SIM_KEY_A, 1, true);
    sim_gpio_set_callback(gpio_key_callback, &cb_pin);
    
    sim_gpio_handle_key(SIM_KEY_A, true);
    TEST_ASSERT_EQUAL(1, cb_pin);
    
    sim_gpio_handle_key(SIM_KEY_A, false);
    TEST_ASSERT_EQUAL(1, cb_pin);
}

void test_multiple_keys_different_pins(void) {
    sim_gpio_register(0, false);
    sim_gpio_register(1, false);
    sim_gpio_register(2, false);
    
    sim_gpio_set_key_mapping(SIM_KEY_UP, 0, true);
    sim_gpio_set_key_mapping(SIM_KEY_DOWN, 1, true);
    sim_gpio_set_key_mapping(SIM_KEY_LEFT, 2, true);
    
    sim_gpio_handle_key(SIM_KEY_UP, true);
    TEST_ASSERT_TRUE(sim_gpio_read(0));
    TEST_ASSERT_FALSE(sim_gpio_read(1));
    TEST_ASSERT_FALSE(sim_gpio_read(2));
    
    sim_gpio_handle_key(SIM_KEY_DOWN, true);
    TEST_ASSERT_TRUE(sim_gpio_read(0));
    TEST_ASSERT_TRUE(sim_gpio_read(1));
    TEST_ASSERT_FALSE(sim_gpio_read(2));
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_gpio_key_mapping);
    RUN_TEST(test_gpio_key_callback);
    RUN_TEST(test_multiple_keys_different_pins);
    
    return UNITY_END();
}