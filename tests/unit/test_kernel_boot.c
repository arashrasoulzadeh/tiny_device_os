#include "unity.h"
#include "scheduler.h"
#include <string.h>

static task_tcb_t* g_task1 = NULL;
static task_tcb_t* g_task2 = NULL;
static int g_task1_runs = 0;
static int g_task2_runs = 0;

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

void setUp(void) {
    g_task1 = NULL;
    g_task2 = NULL;
    g_task1_runs = 0;
    g_task2_runs = 0;
    scheduler_init();
}

void tearDown(void) {
    if (g_task1) task_delete(g_task1);
    if (g_task2) task_delete(g_task2);
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
}

void test_task_yield_should_switch_context(void) {
    task_create("task1", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);
    task_create("task2", task2_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task2);
    
    scheduler_start();
    
    task_yield();
    
    TEST_ASSERT_EQUAL(2, g_task1_runs + g_task2_runs);
}

void test_task_sleep_should_block(void) {
    task_create("task1", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);
    
    scheduler_start();
    
    task_sleep(10);
    
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
    task_create("task1", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &g_task1);
    
    scheduler_start();
    
    task_sleep(1);
    TEST_ASSERT_EQUAL(TASK_STATE_BLOCKED, task_get_state(g_task1));
    
    scheduler_tick();
    TEST_ASSERT_EQUAL(TASK_STATE_READY, task_get_state(g_task1));
}

void test_priority_scheduling(void) {
    task_create("low", task1_entry, NULL, TASK_PRIO_LOW, 256, &g_task1);
    task_create("high", task2_entry, NULL, TASK_PRIO_HIGH, 256, &g_task2);
    
    scheduler_start();
    
    task_tcb_t* current = task_get_current();
    TEST_ASSERT_EQUAL(TASK_PRIO_HIGH, task_get_priority(current));
}

void test_max_tasks_limit(void) {
    task_tcb_t* tasks[MAX_TASKS + 2];
    
    for (int i = 0; i < MAX_TASKS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "task%d", i);
        int ret = task_create(name, task1_entry, NULL, TASK_PRIO_NORMAL, 256, &tasks[i]);
        TEST_ASSERT_EQUAL(0, ret);
    }
    
    int ret = task_create("extra", task1_entry, NULL, TASK_PRIO_NORMAL, 256, &tasks[MAX_TASKS]);
    TEST_ASSERT_NOT_EQUAL(0, ret);
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