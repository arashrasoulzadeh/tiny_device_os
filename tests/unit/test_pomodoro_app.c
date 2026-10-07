#include "unity.h"
#include "power.h"
#include "scheduler.h"

#include <string.h>

void pomodoro_set_worker_demand(task_tcb_t* task);

void setUp(void) {}
void tearDown(void) {}

void test_pomodoro_worker_demand_is_low(void) {
    task_tcb_t task;
    memset(&task, 0, sizeof(task));
    pomodoro_set_worker_demand(&task);
    TEST_ASSERT_EQUAL(POWER_DEMAND_LOW, power_get_demand(&task));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pomodoro_worker_demand_is_low);
    return UNITY_END();
}
