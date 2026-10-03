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
 * This exercises all six end to end for real, through driver_core + the
 * actual sim HAL backends (hal_gpio_sim/hal_i2c_sim/hal_spi_sim/
 * hal_uart_sim/hal_wifi_sim/hal_net_sim/hal_display_sim) - not mocks. */
extern int gpio_driver_init(void);
extern void gpio_driver_deinit(void);
extern int gpio_create_device(int pin_number, const char* name);

extern int i2c_driver_init(void);
extern void i2c_driver_deinit(void);
extern int i2c_create_device(uint8_t addr, const char* name);

extern int spi_driver_init(void);
extern void spi_driver_deinit(void);
extern int spi_create_device(uint8_t cs_pin, const char* name);

extern int uart_driver_init(void);
extern void uart_driver_deinit(void);
extern int uart_create_device(const char* name);

extern int wifi_driver_init(void);
extern void wifi_driver_deinit(void);
extern int wifi_create_device(const char* name);

extern int display_driver_init(void);
extern void display_driver_deinit(void);
extern int display_create_device(const char* name);

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

void test_spi_device_open_and_cs_ioctls_work(void) {
    TEST_ASSERT_EQUAL(0, spi_driver_init());
    TEST_ASSERT_EQUAL(0, spi_create_device(3, "flash"));

    void* handle = NULL;
    TEST_ASSERT_EQUAL(0, device_open("/dev/spi3", &handle));
    TEST_ASSERT_NOT_NULL(handle);

    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x23 /* SPI_CS_ASSERT */, NULL));
    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x24 /* SPI_CS_DEASSERT */, NULL));

    uint8_t new_cs = 7;
    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x25 /* SPI_SET_CS */, &new_cs));

    device_close(handle);
    spi_driver_deinit();
}

void test_uart_device_write_read_round_trips(void) {
    TEST_ASSERT_EQUAL(0, uart_driver_init());
    TEST_ASSERT_EQUAL(0, uart_create_device("console"));

    void* handle = NULL;
    TEST_ASSERT_EQUAL(0, device_open("/dev/uart0", &handle));
    TEST_ASSERT_NOT_NULL(handle);

    const char* msg = "hi";
    TEST_ASSERT_EQUAL(2, device_write(handle, msg, 2));

    int avail = -1;
    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x33 /* UART_GET_RX_AVAILABLE */, &avail));
    TEST_ASSERT_TRUE(avail >= 0);

    device_close(handle);
    uart_driver_deinit();
}

void test_wifi_device_open_and_mode_ioctls_work(void) {
    TEST_ASSERT_EQUAL(0, wifi_driver_init());
    TEST_ASSERT_EQUAL(0, wifi_create_device("sta0"));

    void* handle = NULL;
    TEST_ASSERT_EQUAL(0, device_open("/dev/wifi0", &handle));
    TEST_ASSERT_NOT_NULL(handle);

    bool connected = true;
    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x44 /* WIFI_IS_CONNECTED */, &connected));
    TEST_ASSERT_FALSE(connected);  // never connected, sim has no real AP

    device_close(handle);
    wifi_driver_deinit();
}

void test_display_device_open_and_size_ioctl_work(void) {
    TEST_ASSERT_EQUAL(0, display_driver_init());
    TEST_ASSERT_EQUAL(0, display_create_device("oled0"));

    void* handle = NULL;
    TEST_ASSERT_EQUAL(0, device_open("/dev/display0", &handle));
    TEST_ASSERT_NOT_NULL(handle);

    uint16_t size[2] = {0, 0};
    TEST_ASSERT_EQUAL(0, device_ioctl(handle, 0x59 /* DISPLAY_GET_SIZE */, size));
    TEST_ASSERT_EQUAL(128, size[0]);
    TEST_ASSERT_EQUAL(64, size[1]);

    device_close(handle);
    display_driver_deinit();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gpio_device_open_write_read_round_trips);
    RUN_TEST(test_gpio_toggle_ioctl_flips_the_pin);
    RUN_TEST(test_two_gpio_devices_are_independent);
    RUN_TEST(test_i2c_device_write_read_round_trips_through_sim_bus);
    RUN_TEST(test_spi_device_open_and_cs_ioctls_work);
    RUN_TEST(test_uart_device_write_read_round_trips);
    RUN_TEST(test_wifi_device_open_and_mode_ioctls_work);
    RUN_TEST(test_display_device_open_and_size_ioctl_work);
    return UNITY_END();
}
