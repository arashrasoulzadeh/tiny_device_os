#include "unity.h"
#include "scheduler.h"
#include <stdint.h>

static int g_phase = 0;
static task_tcb_t* g_sleeper = NULL;

/* Task that increments across a sleep — requires a real private stack. */
static void sleeper_entry(void* arg) {
    (void)arg;
    g_phase = 1;
    task_sleep(5);
    g_phase = 2;
    task_sleep(5);
    g_phase = 3;
}

void setUp(void) {
    g_phase = 0;
    g_sleeper = NULL;
    scheduler_init();
}

void tearDown(void) {
    /* sleeper may still be runnable/blocked; leave cleanup to process exit */
}

void test_task_sleep_resumes_after_ticks_and_main_continues(void) {
    TEST_ASSERT_EQUAL(0, task_create("sleeper", sleeper_entry, NULL,
                                     TASK_PRIO_NORMAL, 64 * 1024, &g_sleeper));
    TEST_ASSERT_EQUAL(0, scheduler_start());

    /* First step should run sleeper until first sleep */
    scheduler_step();
    TEST_ASSERT_EQUAL(1, g_phase);
    TEST_ASSERT_EQUAL(TASK_STATE_BLOCKED, task_get_state(g_sleeper));

    /* Advance time while main keeps using its own stack */
    for (int i = 0; i < 5; i++) {
        scheduler_tick();
    }
    TEST_ASSERT_EQUAL(TASK_STATE_READY, task_get_state(g_sleeper));

    /* Resume sleeper — must not corrupt stack / crash */
    scheduler_step();
    TEST_ASSERT_EQUAL(2, g_phase);
    TEST_ASSERT_EQUAL(TASK_STATE_BLOCKED, task_get_state(g_sleeper));

    for (int i = 0; i < 5; i++) {
        scheduler_tick();
    }
    scheduler_step();
    TEST_ASSERT_EQUAL(3, g_phase);
}

void test_task_suspend_blocked_does_not_wake_on_tick(void) {
    TEST_ASSERT_EQUAL(0, task_create("sleeper", sleeper_entry, NULL,
                                     TASK_PRIO_NORMAL, 64 * 1024, &g_sleeper));
    TEST_ASSERT_EQUAL(0, scheduler_start());

    scheduler_step();
    TEST_ASSERT_EQUAL(TASK_STATE_BLOCKED, task_get_state(g_sleeper));

    task_suspend(g_sleeper);
    TEST_ASSERT_EQUAL(TASK_STATE_SUSPENDED, task_get_state(g_sleeper));

    for (int i = 0; i < 20; i++) {
        scheduler_tick();
    }
    TEST_ASSERT_EQUAL(TASK_STATE_SUSPENDED, task_get_state(g_sleeper));
    TEST_ASSERT_EQUAL(1, g_phase);

    task_resume(g_sleeper);
    TEST_ASSERT_EQUAL(TASK_STATE_READY, task_get_state(g_sleeper));
    scheduler_step();
    TEST_ASSERT_EQUAL(2, g_phase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_task_sleep_resumes_after_ticks_and_main_continues);
    RUN_TEST(test_task_suspend_blocked_does_not_wake_on_tick);
    return UNITY_END();
}
