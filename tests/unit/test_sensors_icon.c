#include "unity.h"
#include "icons.h"

extern const app_icon_t sensors_app_icon;

void setUp(void) {}
void tearDown(void) {}

void test_sensors_icon_is_a_16_row_thermometer(void) {
    TEST_ASSERT_EQUAL_HEX16(0x0180, sensors_app_icon.rows[0]);
    TEST_ASSERT_EQUAL_HEX16(0x0FF0, sensors_app_icon.rows[7]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sensors_icon_is_a_16_row_thermometer);
    return UNITY_END();
}
