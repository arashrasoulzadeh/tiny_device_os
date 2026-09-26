#include "unity.h"
#include "scheduler.h"
#include <stdio.h>
#include <string.h>

static task_tcb_t* g_task1 = NULL;
static task_tcb_t* g_task2 = NULL;
static int g_task1_runs = 0;
static int g_task2_runs = 0;
static int g_sleeper_phase = 0;

static void task1_entry(void* arg) {
    (void)arg;
    g_task1_runs++;
    task_yield();
    g_task1_runs++;
}

static void task2_entry(void* arg) {
    (void)arg;
    g_task2_runs++;
    task_yield();
    g_task2_runs++;
}

static void sleeper_entry(void* arg) {
    (void)arg;
    g_sleeper_phase = 1;
    task_sleep(3);
    g_sleeper_phase = 2;
}

void setUp(void) {
    g_task1 = NULL;
    g_task2 = NULL;
    g_task1_runs = 0;
    g_task2_runs = 0;
    g_sleeper_phase = 0;
    scheduler_init();
}

void tearDown(void) {
    if (g_task1 && task_get_state(g_task1) != TASK_STATE_TERMINATED) {
        task_delete(g_task1);
    }
    if (g_task2 && task_get_state(g_task2) != TASK_STATE_TERMINATED) {
        task_delete(g_task2);
    }
    g_task1 = NULL;
    g_task2 = NULL;
}

void test_scheduler_init_should_succeed(void) {
    TEST_ASSERT_EQUAL(0, scheduler_init());
}

void test_task_create_should_succeed(void) {
    int ret = task_create("task1", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_NOT_NULL(g_task1);
    TEST_ASSERT_EQUAL_STRING("task1", task_get_name(g_task1));
    TEST_ASSERT_EQUAL(TASK_PRIO_NORMAL, task_get_priority(g_task1));
    TEST_ASSERT_EQUAL(TASK_STATE_READY, task_get_state(g_task1));
}

void test_task_create_with_invalid_priority_should_fail(void) {
    int ret = task_create("task1", task1_entry, NULL, (task_priority_t)10, 256, &g_task1);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

void test_task_delete_should_work(void) {
    int ret = task_create("task1", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);
    TEST_ASSERT_EQUAL(0, ret);

    ret = task_delete(g_task1);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(TASK_STATE_TERMINATED, task_get_state(g_task1));
    g_task1 = NULL;
}

void test_task_yield_should_switch_context(void) {
    task_create("task1", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);
    task_create("task2", task2_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task2);

    scheduler_start();
    /* Drain both cooperative tasks to completion. */
    for (int i = 0; i < 8; i++) {
        scheduler_step();
    }

    TEST_ASSERT_EQUAL(2, g_task1_runs);
    TEST_ASSERT_EQUAL(2, g_task2_runs);
}

void test_task_sleep_should_block(void) {
    task_create("sleeper", sleeper_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);
    scheduler_start();
    scheduler_step();
    TEST_ASSERT_EQUAL(1, g_sleeper_phase);
    TEST_ASSERT_EQUAL(TASK_STATE_BLOCKED, task_get_state(g_task1));
}

void test_task_suspend_resume_should_work(void) {
    task_create("task1", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);

    task_suspend(g_task1);
    TEST_ASSERT_EQUAL(TASK_STATE_SUSPENDED, task_get_state(g_task1));

    task_resume(g_task1);
    TEST_ASSERT_EQUAL(TASK_STATE_READY, task_get_state(g_task1));
}

void test_scheduler_tick_should_wake_sleeping_tasks(void) {
    task_create("sleeper", sleeper_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);

    scheduler_start();
    scheduler_step();
    TEST_ASSERT_EQUAL(TASK_STATE_BLOCKED, task_get_state(g_task1));

    for (int i = 0; i < 3; i++) {
        scheduler_tick();
    }
    TEST_ASSERT_EQUAL(TASK_STATE_READY, task_get_state(g_task1));

    scheduler_step();
    TEST_ASSERT_EQUAL(2, g_sleeper_phase);
}

void test_priority_scheduling(void) {
    task_create("low", task1_entry, NULL, TASK_PRIO_LOW, 256, &g_task1);
    task_create("high", task2_entry, NULL, TASK_PRIO_HIGH, 256, &g_task2);

    scheduler_start();
    /* High-priority task should have run first. */
    TEST_ASSERT_TRUE(g_task2_runs >= 1);
}

void test_max_tasks_limit(void) {
    /* idle + main already consume 2 TCBs inside scheduler_init. */
    task_tcb_t* tasks[MAX_TASKS];
    int created = 0;

    for (int i = 0; i < MAX_TASKS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "task%d", i);
        int ret = task_create(name, task1_entry, NULL, TASK_PRIO_NORMAL, 256, &tasks[i]);
        if (ret != 0) {
            break;
        }
        created++;
    }

    TEST_ASSERT_TRUE(created >= 1);
    TEST_ASSERT_TRUE(created < MAX_TASKS);

    task_tcb_t* extra = NULL;
    TEST_ASSERT_NOT_EQUAL(0, task_create("extra", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &extra));

    for (int i = 0; i < created; i++) {
        task_delete(tasks[i]);
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_scheduler_init_should_succeed);
    RUN_TEST(test_task_create_should_succeed);
    RUN_TEST(test_task_create_with_invalid_priority_should_fail);
    RUN_TEST(test_task_delete_should_work);
    RUN_TEST(test_task_yield_should_switch_context);
    RUN_TEST(test_task_sleep_should_block);
    RUN_TEST(test_task_suspend_resume_should_work);
    RUN_TEST(test_scheduler_tick_should_wake_sleeping_tasks);
    RUN_TEST(test_priority_scheduling);
    RUN_TEST(test_max_tasks_limit);

    return UNITY_END();
}
