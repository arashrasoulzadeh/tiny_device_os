#include "unity.h"
#include "sensors_app.h"
#include "power.h"

#include <string.h>

power_demand_t sensors_app_power_demand(void);

void setUp(void) {}
void tearDown(void) {}

void test_format_line_shows_chip_temperature_in_celsius(void) {
    char buf[32];
    TEST_ASSERT_EQUAL(10, sensors_format_line(buf, sizeof(buf), "temp", SENSOR_TYPE_TEMP, 253));
    TEST_ASSERT_EQUAL_STRING("temp 25.3C", buf);
    TEST_ASSERT_EQUAL(0, sensors_format_line(buf, sizeof(buf), "temp", SENSOR_TYPE_TEMP, -15) > 0
                             ? 0
                             : -1);
    TEST_ASSERT_EQUAL_STRING("temp -1.5C", buf);
}

void test_format_line_shows_cpu_ram_and_clock(void) {
    char buf[32];
    TEST_ASSERT_TRUE(sensors_format_line(buf, sizeof(buf), "cpu", SENSOR_TYPE_CPU, 12) > 0);
    TEST_ASSERT_EQUAL_STRING("cpu 12%", buf);
    TEST_ASSERT_TRUE(sensors_format_line(buf, sizeof(buf), "ram", SENSOR_TYPE_RAM, 46) > 0);
    TEST_ASSERT_EQUAL_STRING("ram 46%", buf);
    TEST_ASSERT_TRUE(sensors_format_line(buf, sizeof(buf), "pwr", SENSOR_TYPE_POWER, 160) > 0);
    TEST_ASSERT_EQUAL_STRING("pwr 160MHz", buf);
    TEST_ASSERT_TRUE(sensors_format_line(buf, sizeof(buf), "pwr", SENSOR_TYPE_POWER, -1) > 0);
    TEST_ASSERT_EQUAL_STRING("pwr --", buf);
}

void test_sensors_asks_for_low_compute(void) {
    TEST_ASSERT_EQUAL(POWER_DEMAND_LOW, sensors_app_power_demand());
}

void test_format_line_shows_an_adc_count(void) {
    char buf[32];
    TEST_ASSERT_TRUE(sensors_format_line(buf, sizeof(buf), "light", SENSOR_TYPE_ADC, 2048) > 0);
    TEST_ASSERT_EQUAL_STRING("light 2048", buf);
    TEST_ASSERT_EQUAL(-1, sensors_format_line(NULL, 8, "temp", SENSOR_TYPE_TEMP, 250));
    TEST_ASSERT_EQUAL(-1, sensors_format_line(buf, sizeof(buf), NULL, SENSOR_TYPE_TEMP, 250));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_format_line_shows_chip_temperature_in_celsius);
    RUN_TEST(test_format_line_shows_cpu_ram_and_clock);
    RUN_TEST(test_sensors_asks_for_low_compute);
    RUN_TEST(test_format_line_shows_an_adc_count);
    return UNITY_END();
}
