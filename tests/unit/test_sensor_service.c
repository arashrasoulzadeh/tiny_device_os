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

void test_builtin_list_is_empty_without_a_device_config(void) {
    TEST_ASSERT_EQUAL(0, sensor_service_load_builtin());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_get_reads_an_adc_registered_by_key);
    RUN_TEST(test_refresh_ms_keeps_the_cached_sample);
    RUN_TEST(test_zero_refresh_samples_every_read);
    RUN_TEST(test_unknown_or_duplicate_keys_are_rejected);
    RUN_TEST(test_builtin_list_is_empty_without_a_device_config);
    return UNITY_END();
}
