#include "unity.h"
#include "sensor_service.h"

void setUp(void) {
    sensor_service_reset();
}

void tearDown(void) {
    sensor_service_reset();
}

void test_get_reads_an_adc_registered_by_key(void) {
    int32_t value = 0;
    TEST_ASSERT_EQUAL(0, sensor_service_add("temp", SENSOR_TYPE_ADC, "/dev/adc0", 1000));
    TEST_ASSERT_EQUAL(0, sensor_get("temp", &value));
    TEST_ASSERT_EQUAL_INT(2048, value);
    TEST_ASSERT_EQUAL_UINT32(1, sensor_service_samples());
}

void test_refresh_ms_keeps_the_cached_sample(void) {
    int32_t value = 0;
    TEST_ASSERT_EQUAL(0, sensor_service_add("temp", SENSOR_TYPE_ADC, NULL, 1000));
    TEST_ASSERT_EQUAL(0, sensor_get_at("temp", 100, &value));
    TEST_ASSERT_EQUAL(0, sensor_get_at("temp", 1099, &value));
    TEST_ASSERT_EQUAL_UINT32(1, sensor_service_samples());
    TEST_ASSERT_EQUAL_INT(2048, value);

    TEST_ASSERT_EQUAL(0, sensor_get_at("temp", 1100, &value));
    TEST_ASSERT_EQUAL_UINT32(2, sensor_service_samples());
}

void test_zero_refresh_samples_every_read(void) {
    int32_t value = 0;
    TEST_ASSERT_EQUAL(0, sensor_service_add("light", SENSOR_TYPE_ADC, "/dev/adc1", 0));
    TEST_ASSERT_EQUAL(0, sensor_get_at("light", 0, &value));
    TEST_ASSERT_EQUAL(0, sensor_get_at("light", 0, &value));
    TEST_ASSERT_EQUAL_UINT32(2, sensor_service_samples());
}

void test_unknown_or_duplicate_keys_are_rejected(void) {
    int32_t value = 0;
    TEST_ASSERT_EQUAL(-1, sensor_get("missing", &value));
    TEST_ASSERT_EQUAL(-1, sensor_service_add(NULL, SENSOR_TYPE_ADC, NULL, 1000));
    TEST_ASSERT_EQUAL(-1, sensor_service_add("temp", (sensor_type_t)0, NULL, 1000));
    TEST_ASSERT_EQUAL(0, sensor_service_add("temp", SENSOR_TYPE_ADC, NULL, 500));
    TEST_ASSERT_EQUAL(-1, sensor_service_add("temp", SENSOR_TYPE_ADC, NULL, 500));
}

void test_chip_temperature_is_a_cached_sensor(void) {
    int32_t value = 0;
    TEST_ASSERT_EQUAL(0, sensor_service_add("temp", SENSOR_TYPE_TEMP, NULL, 1000));
    TEST_ASSERT_EQUAL(1, sensor_service_count());
    TEST_ASSERT_EQUAL_STRING("temp", sensor_service_key(0));
    TEST_ASSERT_EQUAL(SENSOR_TYPE_TEMP, sensor_service_type(0));
    TEST_ASSERT_EQUAL(0, sensor_get_at("temp", 0, &value));
    TEST_ASSERT_EQUAL_INT(250, value);
    TEST_ASSERT_EQUAL(0, sensor_get_at("temp", 500, &value));
    TEST_ASSERT_EQUAL_UINT32(1, sensor_service_samples());
}

void test_builtin_meters_register_without_a_device_config(void) {
    int32_t cpu = -1;
    int32_t ram = -1;
    int32_t pwr = 0;
    TEST_ASSERT_EQUAL(3, sensor_service_load_builtin());
    TEST_ASSERT_EQUAL(3, sensor_service_count());
    TEST_ASSERT_EQUAL_STRING("cpu", sensor_service_key(0));
    TEST_ASSERT_EQUAL(SENSOR_TYPE_CPU, sensor_service_type(0));
    TEST_ASSERT_EQUAL_STRING("ram", sensor_service_key(1));
    TEST_ASSERT_EQUAL(SENSOR_TYPE_RAM, sensor_service_type(1));
    TEST_ASSERT_EQUAL_STRING("pwr", sensor_service_key(2));
    TEST_ASSERT_EQUAL(SENSOR_TYPE_POWER, sensor_service_type(2));
    TEST_ASSERT_EQUAL(0, sensor_get("cpu", &cpu));
    TEST_ASSERT_TRUE(cpu >= 0 && cpu <= 100);
    TEST_ASSERT_EQUAL(0, sensor_get("ram", &ram));
    TEST_ASSERT_TRUE(ram >= 0 && ram <= 100);
    TEST_ASSERT_EQUAL(0, sensor_get("pwr", &pwr));
    TEST_ASSERT_EQUAL_INT(-1, pwr);
}

void test_busy_percent_is_the_share_of_time_not_idle(void) {
    TEST_ASSERT_EQUAL_INT(0, sensor_cpu_busy_percent(0, 0));
    TEST_ASSERT_EQUAL_INT(0, sensor_cpu_busy_percent(100, 100));
    TEST_ASSERT_EQUAL_INT(0, sensor_cpu_busy_percent(101, 100));
    TEST_ASSERT_EQUAL_INT(50, sensor_cpu_busy_percent(50, 100));
    TEST_ASSERT_EQUAL_INT(100, sensor_cpu_busy_percent(0, 100));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_get_reads_an_adc_registered_by_key);
    RUN_TEST(test_refresh_ms_keeps_the_cached_sample);
    RUN_TEST(test_zero_refresh_samples_every_read);
    RUN_TEST(test_unknown_or_duplicate_keys_are_rejected);
    RUN_TEST(test_chip_temperature_is_a_cached_sensor);
    RUN_TEST(test_builtin_meters_register_without_a_device_config);
    RUN_TEST(test_busy_percent_is_the_share_of_time_not_idle);
    return UNITY_END();
}
