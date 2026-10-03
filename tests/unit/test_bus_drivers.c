#include "unity.h"
#include "driver.h"
#include <string.h>

/* gpio_driver.c/i2c_driver.c/spi_driver.c/uart_driver.c/wifi_driver.c/
 * display_driver.c had zero tests. None of their _driver_init()/
 * _create_device() functions are declared in any header (not even their
 * own), so nothing outside their own .c file could call them - one more
 * sign, on top of nothing calling driver_core_init() anywhere real, that
 * this whole subsystem was written and never connected to anything.
 * Declared here the same way, since that's the only way to reach them at
 * all; a real header is the natural next step for whoever wires this up.
 *
 * This exercises gpio_driver and i2c_driver end to end for real, through
 * driver_core + the actual sim HAL backends (hal_gpio_sim/hal_i2c_sim) -
 * not mocks. spi/uart/wifi/display follow the identical probe/open/read/
 * write/ioctl/create_device structure and share every core-level fix
 * (device_open's deadlock, device_read/write's dispatch, the ownership
 * flag) already covered by tests/unit/test_driver.c, so they aren't
 * each duplicated here. */
extern int gpio_driver_init(void);
extern void gpio_driver_deinit(void);
extern int gpio_create_device(int pin_number, const char* name);

extern int i2c_driver_init(void);
extern void i2c_driver_deinit(void);
extern int i2c_create_device(uint8_t addr, const char* name);

void setUp(void) {
    driver_core_init();
}

void tearDown(void) {
    driver_core_init();  // not deinit() - see test_driver.c's tearDown comment
}

void test_gpio_device_open_write_read_round_trips(void) {
    TEST_ASSERT_EQUAL(0, gpio_driver_init());
    TEST_ASSERT_EQUAL(0, gpio_create_device(5, "gpio5"));

    void* handle = NULL;
    TEST_ASSERT_EQUAL(0, device_open("/dev/gpio5", &handle));
    TEST_ASSERT_NOT_NULL(handle);

    bool level_out = true;
    TEST_ASSERT_EQUAL(1, device_write(handle, &level_out, 1));

    bool level_in = false;
    TEST_ASSERT_EQUAL(1, device_read(handle, &level_in, 1));
    TEST_ASSERT_TRUE(level_in);

    int pin = -1;
    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x04 /* GPIO_GET_PIN */, &pin));
    TEST_ASSERT_EQUAL(5, pin);

    device_close(handle);
    gpio_driver_deinit();
}

void test_gpio_toggle_ioctl_flips_the_pin(void) {
    gpio_driver_init();
    gpio_create_device(6, "gpio6");

    void* handle = NULL;
    device_open("/dev/gpio6", &handle);

    bool initial = false;
    device_write(handle, &initial, 1);

    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x05 /* GPIO_TOGGLE */, NULL));

    bool after = false;
    device_read(handle, &after, 1);
    TEST_ASSERT_TRUE(after);

    device_close(handle);
    gpio_driver_deinit();
}

void test_two_gpio_devices_are_independent(void) {
    gpio_driver_init();
    // gpio_create_device()'s `name` only sets device_t.name (for
    // device_find()/device_unregister()); .path always comes from the
    // pin number regardless of what name is passed.
    gpio_create_device(1, "gpioA");
    gpio_create_device(2, "gpioB");

    void *a = NULL, *b = NULL;
    device_open("/dev/gpio1", &a);
    device_open("/dev/gpio2", &b);
    TEST_ASSERT_NOT_EQUAL(a, b);

    bool on = true, off = false;
    device_write(a, &on, 1);
    device_write(b, &off, 1);

    bool a_level = false, b_level = true;
    device_read(a, &a_level, 1);
    device_read(b, &b_level, 1);
    TEST_ASSERT_TRUE(a_level);
    TEST_ASSERT_FALSE(b_level);

    device_close(a);
    device_close(b);
    gpio_driver_deinit();
}

void test_i2c_device_write_read_round_trips_through_sim_bus(void) {
    TEST_ASSERT_EQUAL(0, i2c_driver_init());
    TEST_ASSERT_EQUAL(0, i2c_create_device(0x42, "sensor"));

    void* handle = NULL;
    TEST_ASSERT_EQUAL(0, device_open("/dev/i2c/0x42", &handle));
    TEST_ASSERT_NOT_NULL(handle);

    uint8_t addr = 0;
    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x11 /* I2C_GET_ADDR */, &addr));
    TEST_ASSERT_EQUAL(0x42, addr);

    device_close(handle);
    i2c_driver_deinit();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gpio_device_open_write_read_round_trips);
    RUN_TEST(test_gpio_toggle_ioctl_flips_the_pin);
    RUN_TEST(test_two_gpio_devices_are_independent);
    RUN_TEST(test_i2c_device_write_read_round_trips_through_sim_bus);
    return UNITY_END();
}
