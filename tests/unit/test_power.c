#include "unity.h"
#include "power.h"
#include "scheduler.h"

/* kernel/power.c had zero tests - flagged repeatedly earlier this session
 * (docs/agent-guide.md calls it out explicitly) as unverified. Found a
 * real, serious bug while writing this: power_light_sleep(0) busy-looped
 * `while (current_mode == LIGHT_SLEEP)` forever - nothing in this module
 * ever sets current_mode away from LIGHT_SLEEP from outside that loop
 * (no power_wake() or equivalent exists), so it was an unconditional,
 * permanent hang. Fixed by rejecting timeout_ms == 0 outright. NEVER
 * call power_light_sleep(0) in a test without that fix in place - it
 * will hang the whole suite, not just one test. */

void setUp(void) {
    scheduler_init();
    power_init();
}

void tearDown(void) {
    power_deinit();
}

void test_init_sets_active_mode(void) {
    TEST_ASSERT_EQUAL(POWER_MODE_ACTIVE, get_power_mode());
    TEST_ASSERT_FALSE(is_in_sleep_mode());
}

void test_init_is_idempotent(void) {
    TEST_ASSERT_EQUAL(0, power_init());
    TEST_ASSERT_EQUAL(0, power_init());
}

/* Regression: this used to hang the process forever. */
void test_light_sleep_rejects_zero_timeout(void) {
    TEST_ASSERT_EQUAL(-1, power_light_sleep(0));
}

void test_light_sleep_with_timeout_returns_to_active(void) {
    TEST_ASSERT_EQUAL(0, power_light_sleep(5));
    TEST_ASSERT_EQUAL(POWER_MODE_ACTIVE, get_power_mode());
    TEST_ASSERT_FALSE(is_in_sleep_mode());
}

void test_light_sleep_before_init_fails(void) {
    power_deinit();
    TEST_ASSERT_EQUAL(-1, power_light_sleep(5));
}

void test_deep_sleep_with_timeout_wakes_via_timer(void) {
    TEST_ASSERT_EQUAL(0, power_deep_sleep(5));
    wake_result_t wr = get_wake_result();
    TEST_ASSERT_TRUE(wr.woke_up);
    TEST_ASSERT_EQUAL(-1, wr.wake_gpio);  // no GPIO configured - timer wake
}

/* Regression: wake_gpio used to be hardcoded -1 regardless of which pin
 * was actually configured as the wake source. */
void test_deep_sleep_with_gpio_wake_records_the_pin(void) {
    TEST_ASSERT_EQUAL(0, power_deep_sleep_with_gpio_wake(7, 0, 5));
    wake_result_t wr = get_wake_result();
    TEST_ASSERT_TRUE(wr.woke_up);
    TEST_ASSERT_EQUAL(7, wr.wake_gpio);
}

void test_driver_registered_with_null_callbacks_does_not_crash_sleep(void) {
    TEST_ASSERT_EQUAL(0, power_register_driver((void*)0x1, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(0, power_deep_sleep(5));
    power_unregister_driver((void*)0x1);
}

static int g_suspend_calls;
static int g_resume_calls;
static void* g_last_suspend_arg;
static void* g_last_resume_arg;

static void test_suspend_cb(void* arg) {
    g_suspend_calls++;
    g_last_suspend_arg = arg;
}
static void test_resume_cb(void* arg) {
    g_resume_calls++;
    g_last_resume_arg = arg;
}

void test_registered_driver_gets_suspend_and_resume_called(void) {
    g_suspend_calls = g_resume_calls = 0;
    int marker = 42;

    TEST_ASSERT_EQUAL(0, power_register_driver(&marker, test_suspend_cb, test_resume_cb, &marker));
    TEST_ASSERT_EQUAL(0, power_deep_sleep(5));

    TEST_ASSERT_EQUAL(1, g_suspend_calls);
    TEST_ASSERT_EQUAL(1, g_resume_calls);
    TEST_ASSERT_EQUAL_PTR(&marker, g_last_suspend_arg);
    TEST_ASSERT_EQUAL_PTR(&marker, g_last_resume_arg);

    power_unregister_driver(&marker);
}

void test_unregistered_driver_is_not_called(void) {
    g_suspend_calls = g_resume_calls = 0;
    int marker = 1;

    power_register_driver(&marker, test_suspend_cb, test_resume_cb, &marker);
    power_unregister_driver(&marker);
    power_deep_sleep(5);

    TEST_ASSERT_EQUAL(0, g_suspend_calls);
    TEST_ASSERT_EQUAL(0, g_resume_calls);
}

void test_cpu_freq_set_get_round_trips_for_valid_frequency(void) {
    TEST_ASSERT_EQUAL(0, power_set_cpu_freq(80));
    TEST_ASSERT_EQUAL(80, power_get_cpu_freq());
}

void test_cpu_freq_rejects_unavailable_frequency(void) {
    TEST_ASSERT_EQUAL(-1, power_set_cpu_freq(999));
    // Rejected - must not have silently changed the current frequency.
    TEST_ASSERT_NOT_EQUAL(999, power_get_cpu_freq());
}

void test_get_available_freqs_returns_the_fixed_table(void) {
    uint32_t freqs[8] = {0};
    int n = power_get_available_freqs(freqs, 8);
    TEST_ASSERT_EQUAL(5, n);
    TEST_ASSERT_EQUAL(240, freqs[0]);
}

void test_set_available_freqs_replaces_the_table(void) {
    static const uint32_t freqs[] = {160, 80, 40};
    uint32_t got[8] = {0};
    TEST_ASSERT_EQUAL(0, power_set_available_freqs(freqs, 3));
    TEST_ASSERT_EQUAL(3, power_get_available_freqs(got, 8));
    TEST_ASSERT_EQUAL(160, got[0]);
    TEST_ASSERT_EQUAL(40, got[2]);
    TEST_ASSERT_EQUAL(-1, power_set_available_freqs(NULL, 3));
    TEST_ASSERT_EQUAL(-1, power_set_available_freqs(freqs, 0));
}

void test_get_available_freqs_respects_max_count(void) {
    uint32_t freqs[2] = {0};
    int n = power_get_available_freqs(freqs, 2);
    TEST_ASSERT_EQUAL(2, n);
}

void test_rtc_time_set_get_round_trips(void) {
    TEST_ASSERT_EQUAL(0, set_rtc_time(123456));
    TEST_ASSERT_EQUAL(123456, get_rtc_time_ms());
}

void test_battery_info_returns_plausible_values(void) {
    power_battery_info_t info;
    TEST_ASSERT_EQUAL(0, power_get_battery_info(&info));
    TEST_ASSERT_TRUE(info.battery_percent >= 0.0f && info.battery_percent <= 100.0f);
}

void test_battery_info_rejects_null(void) {
    TEST_ASSERT_EQUAL(-1, power_get_battery_info(NULL));
}

void test_wake_source_str_known_and_unknown(void) {
    TEST_ASSERT_EQUAL_STRING("TIMER", wake_source_str(POWER_WAKE_TIMER));
    TEST_ASSERT_EQUAL_STRING("GPIO", wake_source_str(POWER_WAKE_GPIO));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", wake_source_str(999));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_sets_active_mode);
    RUN_TEST(test_init_is_idempotent);
    RUN_TEST(test_light_sleep_rejects_zero_timeout);
    RUN_TEST(test_light_sleep_with_timeout_returns_to_active);
    RUN_TEST(test_light_sleep_before_init_fails);
    RUN_TEST(test_deep_sleep_with_timeout_wakes_via_timer);
    RUN_TEST(test_deep_sleep_with_gpio_wake_records_the_pin);
    RUN_TEST(test_driver_registered_with_null_callbacks_does_not_crash_sleep);
    RUN_TEST(test_registered_driver_gets_suspend_and_resume_called);
    RUN_TEST(test_unregistered_driver_is_not_called);
    RUN_TEST(test_cpu_freq_set_get_round_trips_for_valid_frequency);
    RUN_TEST(test_cpu_freq_rejects_unavailable_frequency);
    RUN_TEST(test_get_available_freqs_returns_the_fixed_table);
    RUN_TEST(test_set_available_freqs_replaces_the_table);
    RUN_TEST(test_get_available_freqs_respects_max_count);
    RUN_TEST(test_rtc_time_set_get_round_trips);
    RUN_TEST(test_battery_info_returns_plausible_values);
    RUN_TEST(test_battery_info_rejects_null);
    RUN_TEST(test_wake_source_str_known_and_unknown);
    return UNITY_END();
}
