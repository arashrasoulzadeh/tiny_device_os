#include "unity.h"
#include "scheduler.h"
#include "power.h"
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

/* Self-suspend must save context so resume continues after the suspend call. */
static void self_suspend_entry(void* arg) {
    (void)arg;
    g_phase = 1;
    task_suspend(task_get_current());
    g_phase = 2;
}

void test_task_self_suspend_resumes_after_suspend_point(void) {
    TEST_ASSERT_EQUAL(0, task_create("park", self_suspend_entry, NULL, TASK_PRIO_NORMAL,
                                     64 * 1024, &g_sleeper));
    TEST_ASSERT_EQUAL(0, scheduler_start());

    scheduler_step();
    TEST_ASSERT_EQUAL(1, g_phase);
    TEST_ASSERT_EQUAL(TASK_STATE_SUSPENDED, task_get_state(g_sleeper));

    task_resume(g_sleeper);
    scheduler_step();
    TEST_ASSERT_EQUAL(2, g_phase);
}

/* scheduler_enter_idle()/scheduler_tickless_idle() were entirely
 * untested - idle_task (the only intended caller) is itself unreachable
 * since scheduler_step()/task_yield() both explicitly refuse to switch
 * to a TASK_PRIO_IDLE task, so nothing ever exercised this code. Calling
 * them directly exposed: a short sleep (below deep_sleep_min_ticks)
 * advanced no time in scheduler_enter_idle(), and hung forever in
 * scheduler_tickless_idle() (a busy-wait loop with nothing inside it
 * touching tick_count) - both fixed to advance tick_count directly. */
void test_scheduler_enter_idle_short_sleep_advances_ticks(void) {
    scheduler_set_deep_sleep_min_ticks(10000);
    TEST_ASSERT_EQUAL(0, task_create("sleeper", sleeper_entry, NULL,
                                     TASK_PRIO_NORMAL, 64 * 1024, &g_sleeper));
    TEST_ASSERT_EQUAL(0, scheduler_start());

    scheduler_step();
    TEST_ASSERT_EQUAL(TASK_STATE_BLOCKED, task_get_state(g_sleeper));
    /* Sleeper woke at tick_count + 5, well under deep_sleep_min_ticks.
     * scheduler_tick() is what populates next_wake_tick from the blocked
     * list - call it once to populate it without advancing past the
     * target, then idle should jump straight to that wake tick. */
    scheduler_tick();
    uint32_t target = scheduler_get_next_wake_tick();
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, target);

    scheduler_enter_idle();
    TEST_ASSERT_EQUAL(target, scheduler_get_tick_count());
    TEST_ASSERT_EQUAL(POWER_MODE_ACTIVE, scheduler_get_power_mode());
}

void test_scheduler_tickless_idle_short_sleep_does_not_hang(void) {
    scheduler_set_deep_sleep_min_ticks(10000);
    TEST_ASSERT_EQUAL(0, task_create("sleeper", sleeper_entry, NULL,
                                     TASK_PRIO_NORMAL, 64 * 1024, &g_sleeper));
    TEST_ASSERT_EQUAL(0, scheduler_start());

    scheduler_step();
    scheduler_tick();
    uint32_t target = scheduler_get_next_wake_tick();
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, target);

    /* Would hang forever before the fix - reaching this TEST_ASSERT at
     * all is the regression check. */
    scheduler_tickless_idle();
    TEST_ASSERT_EQUAL(target, scheduler_get_tick_count());
    TEST_ASSERT_EQUAL(POWER_MODE_ACTIVE, scheduler_get_power_mode());
}

void test_scheduler_step_with_only_idle_increments_the_idle_count(void) {
    uint32_t before;
    TEST_ASSERT_EQUAL(0, scheduler_start());
    before = power_governor_idle_steps();
    scheduler_step();
    TEST_ASSERT_EQUAL(before + 1, power_governor_idle_steps());
}

void test_scheduler_exit_idle_resets_to_active(void) {
    scheduler_enable_tickless_idle(true);
    scheduler_enter_deep_sleep(0); /* leaves power_mode == DEEP_SLEEP */
    TEST_ASSERT_EQUAL(POWER_MODE_DEEP_SLEEP, scheduler_get_power_mode());
    scheduler_exit_idle();
    TEST_ASSERT_EQUAL(POWER_MODE_ACTIVE, scheduler_get_power_mode());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_task_sleep_resumes_after_ticks_and_main_continues);
    RUN_TEST(test_task_suspend_blocked_does_not_wake_on_tick);
    RUN_TEST(test_task_self_suspend_resumes_after_suspend_point);
    RUN_TEST(test_scheduler_enter_idle_short_sleep_advances_ticks);
    RUN_TEST(test_scheduler_tickless_idle_short_sleep_does_not_hang);
    RUN_TEST(test_scheduler_step_with_only_idle_increments_the_idle_count);
    RUN_TEST(test_scheduler_exit_idle_resets_to_active);
    return UNITY_END();
}
