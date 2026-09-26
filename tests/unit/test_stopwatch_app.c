#include "unity.h"
#include "stopwatch.h"
#include "scheduler.h"

#include <stdint.h>

static volatile int g_worker_secs;
static volatile bool g_worker_alive;
static task_tcb_t* g_sec_task;

static void probe_sec_task(void* arg) {
    stopwatch_t* sw = (stopwatch_t*)arg;
    while (g_worker_alive) {
        task_sleep(1000);
        if (!g_worker_alive) {
            break;
        }
        if (stopwatch_tick_second(sw)) {
            /* minute wrap — ignore in this probe */
        }
        g_worker_secs = sw->seconds;
    }
}

void setUp(void) {}
void tearDown(void) {}

void test_stopwatch_reset_clears_time_and_stops(void) {
    stopwatch_t sw = {.hours = 1, .minutes = 2, .seconds = 3, .running = true};
    stopwatch_reset(&sw);
    TEST_ASSERT_EQUAL_UINT8(0, sw.hours);
    TEST_ASSERT_EQUAL_UINT8(0, sw.minutes);
    TEST_ASSERT_EQUAL_UINT8(0, sw.seconds);
    TEST_ASSERT_FALSE(sw.running);
}

void test_stopwatch_tick_second_wraps_and_signals_minute(void) {
    stopwatch_t sw;
    stopwatch_reset(&sw);
    sw.running = true;
    sw.seconds = 58;

    TEST_ASSERT_FALSE(stopwatch_tick_second(&sw));
    TEST_ASSERT_EQUAL_UINT8(59, sw.seconds);

    TEST_ASSERT_TRUE(stopwatch_tick_second(&sw));
    TEST_ASSERT_EQUAL_UINT8(0, sw.seconds);
}

void test_stopwatch_tick_minute_wraps_and_signals_hour(void) {
    stopwatch_t sw;
    stopwatch_reset(&sw);
    sw.running = true;
    sw.minutes = 59;

    TEST_ASSERT_TRUE(stopwatch_tick_minute(&sw));
    TEST_ASSERT_EQUAL_UINT8(0, sw.minutes);
}

void test_stopwatch_tick_hour_wraps_at_100(void) {
    stopwatch_t sw;
    stopwatch_reset(&sw);
    sw.running = true;
    sw.hours = 99;

    stopwatch_tick_hour(&sw);
    TEST_ASSERT_EQUAL_UINT8(0, sw.hours);
}

void test_stopwatch_ticks_ignored_when_stopped(void) {
    stopwatch_t sw;
    stopwatch_reset(&sw);
    sw.seconds = 10;
    sw.minutes = 20;
    sw.hours = 3;

    TEST_ASSERT_FALSE(stopwatch_tick_second(&sw));
    TEST_ASSERT_FALSE(stopwatch_tick_minute(&sw));
    stopwatch_tick_hour(&sw);

    TEST_ASSERT_EQUAL_UINT8(10, sw.seconds);
    TEST_ASSERT_EQUAL_UINT8(20, sw.minutes);
    TEST_ASSERT_EQUAL_UINT8(3, sw.hours);
}

void test_stopwatch_null_is_safe(void) {
    stopwatch_reset(NULL);
    TEST_ASSERT_FALSE(stopwatch_tick_second(NULL));
    TEST_ASSERT_FALSE(stopwatch_tick_minute(NULL));
    stopwatch_tick_hour(NULL);
}

void test_stopwatch_sec_worker_advances_after_1000_ticks(void) {
    stopwatch_t sw;
    stopwatch_reset(&sw);
    sw.running = true;
    g_worker_secs = 0;
    g_worker_alive = true;
    g_sec_task = NULL;

    TEST_ASSERT_EQUAL(0, scheduler_init());
    TEST_ASSERT_EQUAL(0, scheduler_start());
    TEST_ASSERT_EQUAL(0, task_create("sw_sec", probe_sec_task, &sw, TASK_PRIO_NORMAL,
                                     64 * 1024, &g_sec_task));

    for (int i = 0; i < 1000; i++) {
        scheduler_step();
        scheduler_tick();
    }
    scheduler_step();

    TEST_ASSERT_EQUAL(1, g_worker_secs);
    TEST_ASSERT_EQUAL_UINT8(1, sw.seconds);

    g_worker_alive = false;
    task_delete(g_sec_task);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stopwatch_reset_clears_time_and_stops);
    RUN_TEST(test_stopwatch_tick_second_wraps_and_signals_minute);
    RUN_TEST(test_stopwatch_tick_minute_wraps_and_signals_hour);
    RUN_TEST(test_stopwatch_tick_hour_wraps_at_100);
    RUN_TEST(test_stopwatch_ticks_ignored_when_stopped);
    RUN_TEST(test_stopwatch_null_is_safe);
    RUN_TEST(test_stopwatch_sec_worker_advances_after_1000_ticks);
    return UNITY_END();
}
