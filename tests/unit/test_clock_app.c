#include "unity.h"
#include "power.h"

power_demand_t clock_app_power_demand(void);

void setUp(void) {}
void tearDown(void) {}

void test_clock_asks_for_low_compute(void) {
    TEST_ASSERT_EQUAL(POWER_DEMAND_LOW, clock_app_power_demand());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clock_asks_for_low_compute);
    return UNITY_END();
}
