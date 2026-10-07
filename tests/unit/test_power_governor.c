#include "unity.h"
#include "power.h"
#include "os_time.h"

#include <string.h>

static uint32_t g_now;
static uint8_t g_bright;
static int g_temp_c;
static int g_temp_ok;

static int set_bright(uint8_t cap) {
    g_bright = cap;
    return 0;
}

static int read_temp(int32_t* temp_c) {
    if (!g_temp_ok || !temp_c) {
        return -1;
    }
    *temp_c = g_temp_c;
    return 0;
}

static void window(int busy, int idle, task_tcb_t* task, bool runnable, uint32_t wake_ms) {
    int i;
    for (i = 0; i < busy; i++) {
        power_governor_note_busy(task);
    }
    for (i = 0; i < idle; i++) {
        power_governor_note_idle();
    }
    power_governor_tick(g_now, wake_ms, runnable);
    g_now += 100;
}

static uint32_t lowest_freq(void) {
    uint32_t freqs[8];
    int n = power_get_available_freqs(freqs, 8);
    uint32_t min = freqs[0];
    int i;
    for (i = 1; i < n; i++) {
        if (freqs[i] < min) {
            min = freqs[i];
        }
    }
    return min;
}

static uint32_t highest_freq(void) {
    uint32_t freqs[8];
    int n = power_get_available_freqs(freqs, 8);
    uint32_t max = freqs[0];
    int i;
    for (i = 1; i < n; i++) {
        if (freqs[i] > max) {
            max = freqs[i];
        }
    }
    return max;
}

void setUp(void) {
    g_now = 0;
    g_bright = 0;
    g_temp_c = 0;
    g_temp_ok = 0;
    power_init();
    power_governor_set_actuators(NULL, set_bright, read_temp);
}

void tearDown(void) {
    power_deinit();
}

void test_quiet_low_demand_ends_at_economy(void) {
    power_governor_set_foreground_demand(POWER_DEMAND_LOW);
    window(0, 4, NULL, true, 0);
    window(0, 4, NULL, true, 0);
    window(0, 4, NULL, true, 0);

    TEST_ASSERT_EQUAL(POWER_LEVEL_ECONOMY, power_governor_get_level());
    TEST_ASSERT_EQUAL(40, power_get_cpu_freq());
    TEST_ASSERT_EQUAL(64, g_bright);
    TEST_ASSERT_LESS_THAN(highest_freq(), power_get_cpu_freq());
}

void test_one_busy_window_steps_up_and_quiet_windows_step_down(void) {
    power_governor_set_foreground_demand(POWER_DEMAND_NORMAL);
    window(1, 3, NULL, true, 0);
    TEST_ASSERT_EQUAL(POWER_LEVEL_PERFORMANCE, power_governor_get_level());
    TEST_ASSERT_EQUAL(highest_freq(), power_get_cpu_freq());
    TEST_ASSERT_EQUAL(255, g_bright);

    window(0, 4, NULL, true, 0);
    window(0, 4, NULL, true, 0);
    window(0, 4, NULL, true, 0);
    TEST_ASSERT_EQUAL(POWER_LEVEL_BALANCED, power_governor_get_level());
    TEST_ASSERT_EQUAL(80, power_get_cpu_freq());
    TEST_ASSERT_EQUAL(180, g_bright);
}

void test_sustained_normal_drops_a_level_and_high_keeps_the_top(void) {
    task_tcb_t high;
    int i;

    memset(&high, 0, sizeof(high));
    TEST_ASSERT_EQUAL(0, power_set_demand(&high, POWER_DEMAND_HIGH));

    power_governor_set_foreground_demand(POWER_DEMAND_NORMAL);
    for (i = 0; i < 4; i++) {
        window(8, 0, NULL, true, 0);
    }
    TEST_ASSERT_EQUAL(POWER_LEVEL_BALANCED, power_governor_get_level());
    TEST_ASSERT_LESS_THAN(highest_freq(), power_get_cpu_freq());

    power_governor_reset();
    power_governor_set_actuators(NULL, set_bright, read_temp);
    power_governor_set_foreground_demand(POWER_DEMAND_NORMAL);
    g_now = 0;
    for (i = 0; i < 4; i++) {
        window(8, 0, &high, true, 0);
    }
    TEST_ASSERT_EQUAL(POWER_LEVEL_PERFORMANCE, power_governor_get_level());
    TEST_ASSERT_EQUAL(highest_freq(), power_get_cpu_freq());
}

void test_hot_die_forces_cool_and_recovers_below_the_low_threshold(void) {
    task_tcb_t high;
    int i;

    memset(&high, 0, sizeof(high));
    power_set_demand(&high, POWER_DEMAND_HIGH);
    g_temp_ok = 1;
    g_temp_c = 75;

    window(8, 0, &high, true, 0);
    TEST_ASSERT_EQUAL(POWER_LEVEL_COOL, power_governor_get_level());
    TEST_ASSERT_EQUAL(lowest_freq(), power_get_cpu_freq());
    TEST_ASSERT_EQUAL(16, g_bright);

    g_temp_c = 55;
    for (i = 0; i < 3; i++) {
        window(1, 3, &high, true, 0);
    }
    TEST_ASSERT_EQUAL(POWER_LEVEL_PERFORMANCE, power_governor_get_level());
    TEST_ASSERT_EQUAL(highest_freq(), power_get_cpu_freq());
}

void test_missing_temperature_leaves_the_thermal_rule_idle(void) {
    g_temp_ok = 0;
    power_governor_set_foreground_demand(POWER_DEMAND_NORMAL);
    window(0, 4, NULL, true, 0);
    window(0, 4, NULL, true, 0);
    window(0, 4, NULL, true, 0);

    TEST_ASSERT_NOT_EQUAL(POWER_LEVEL_COOL, power_governor_get_level());
    TEST_ASSERT_NOT_EQUAL(lowest_freq(), power_get_cpu_freq());
}

void test_idle_far_wake_recommends_light_sleep_without_sleeping(void) {
    uint32_t before = time_now_ms();

    power_governor_set_foreground_demand(POWER_DEMAND_LOW);
    window(0, 4, NULL, false, 200);

    TEST_ASSERT_EQUAL(200, power_governor_light_sleep_ms());
    TEST_ASSERT_EQUAL(POWER_MODE_ACTIVE, get_power_mode());
    TEST_ASSERT_FALSE(is_in_sleep_mode());
    /* power_light_sleep() would block on the wall clock for the whole gap. */
    TEST_ASSERT_UINT32_WITHIN(100, before, time_now_ms());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_quiet_low_demand_ends_at_economy);
    RUN_TEST(test_one_busy_window_steps_up_and_quiet_windows_step_down);
    RUN_TEST(test_sustained_normal_drops_a_level_and_high_keeps_the_top);
    RUN_TEST(test_hot_die_forces_cool_and_recovers_below_the_low_threshold);
    RUN_TEST(test_missing_temperature_leaves_the_thermal_rule_idle);
    RUN_TEST(test_idle_far_wake_recommends_light_sleep_without_sleeping);
    return UNITY_END();
}
