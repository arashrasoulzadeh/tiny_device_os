#include "unity.h"
#include "hal_gpio.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_gpio_open_close(void) {
    hal_gpio_t* gpio = hal_gpio_open("/dev/gpio0", HAL_GPIO_MODE_OUTPUT);
    TEST_ASSERT_NOT_NULL(gpio);
    
    hal_gpio_close(gpio);
}

void test_gpio_write_read(void) {
    hal_gpio_t* gpio = hal_gpio_open("/dev/gpio0", HAL_GPIO_MODE_OUTPUT);
    TEST_ASSERT_NOT_NULL(gpio);
    
    hal_gpio_write(gpio, true);
    TEST_ASSERT_TRUE(hal_gpio_read(gpio));
    
    hal_gpio_write(gpio, false);
    TEST_ASSERT_FALSE(hal_gpio_read(gpio));
    
    hal_gpio_close(gpio);
}

void test_gpio_toggle(void) {
    hal_gpio_t* gpio = hal_gpio_open("/dev/gpio0", HAL_GPIO_MODE_OUTPUT);
    TEST_ASSERT_NOT_NULL(gpio);
    
    hal_gpio_write(gpio, false);
    hal_gpio_toggle(gpio);
    TEST_ASSERT_TRUE(hal_gpio_read(gpio));
    
    hal_gpio_toggle(gpio);
    TEST_ASSERT_FALSE(hal_gpio_read(gpio));
    
    hal_gpio_close(gpio);
}

void test_gpio_modes(void) {
    hal_gpio_t* gpio_in = hal_gpio_open("/dev/gpio1", HAL_GPIO_MODE_INPUT);
    TEST_ASSERT_NOT_NULL(gpio_in);
    
    hal_gpio_t* gpio_out = hal_gpio_open("/dev/gpio2", HAL_GPIO_MODE_OUTPUT);
    TEST_ASSERT_NOT_NULL(gpio_out);
    
    hal_gpio_t* gpio_pu = hal_gpio_open("/dev/gpio3", HAL_GPIO_MODE_INPUT_PULLUP);
    TEST_ASSERT_NOT_NULL(gpio_pu);
    
    hal_gpio_t* gpio_pd = hal_gpio_open("/dev/gpio4", HAL_GPIO_MODE_INPUT_PULLDOWN);
    TEST_ASSERT_NOT_NULL(gpio_pd);
    
    hal_gpio_close(gpio_in);
    hal_gpio_close(gpio_out);
    hal_gpio_close(gpio_pu);
    hal_gpio_close(gpio_pd);
}

static void gpio_irq_callback(int pin, void* arg) {
    (void)pin;
    *(int*)arg = 1;
}

void test_gpio_irq(void) {
    hal_gpio_t* gpio = hal_gpio_open("/dev/gpio0", HAL_GPIO_MODE_INPUT);
    TEST_ASSERT_NOT_NULL(gpio);
    
    int cb_called = 0;
    hal_gpio_set_irq(gpio, HAL_GPIO_IRQ_RISING, gpio_irq_callback, &cb_called);
    
    hal_gpio_enable_irq(gpio);
    
    hal_gpio_close(gpio);
}

void test_gpio_get_info(void) {
    hal_gpio_t* gpio = hal_gpio_open("/dev/gpio5", HAL_GPIO_MODE_OUTPUT);
    TEST_ASSERT_NOT_NULL(gpio);
    
    int pin = hal_gpio_get_pin_number(gpio);
    TEST_ASSERT_GREATER_OR_EQUAL(0, pin);
    
    const char* path = hal_gpio_get_path(gpio);
    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_EQUAL_STRING("/dev/gpio5", path);
    
    hal_gpio_close(gpio);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_gpio_open_close);
    RUN_TEST(test_gpio_write_read);
    RUN_TEST(test_gpio_toggle);
    RUN_TEST(test_gpio_modes);
    RUN_TEST(test_gpio_irq);
    RUN_TEST(test_gpio_get_info);
    
    return UNITY_END();
}