#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include "unity.h"
#include "os_time.h"

static void test_sleep_us(long us) {
    struct timespec ts = {
        .tv_sec = us / 1000000,
        .tv_nsec = (us % 1000000) * 1000,
    };
    nanosleep(&ts, NULL);
}

void setUp(void) {
}

void tearDown(void) {
}

void test_time_now_us_should_increase(void) {
    time_us_t t1 = time_now_us();
    test_sleep_us(1000);
    time_us_t t2 = time_now_us();
    
    TEST_ASSERT_GREATER_THAN_UINT64(t1, t2);
    TEST_ASSERT_GREATER_THAN_UINT64(500, t2 - t1);
}

void test_time_now_ms_should_increase(void) {
    time_ms_t t1 = time_now_ms();
    test_sleep_us(2000);
    time_ms_t t2 = time_now_ms();
    
    TEST_ASSERT_GREATER_THAN_UINT32(t1, t2);
    TEST_ASSERT_GREATER_THAN_UINT32(1, t2 - t1);
}

void test_time_since_us(void) {
    time_us_t start = time_now_us();
    test_sleep_us(1000);
    time_us_t elapsed = time_since_us(start);
    
    TEST_ASSERT_GREATER_THAN_UINT64(500, elapsed);
}

void test_time_sleep_us(void) {
    time_us_t start = time_now_us();
    time_sleep_us(5000);
    time_us_t elapsed = time_since_us(start);
    
    TEST_ASSERT_GREATER_THAN_UINT64(4000, elapsed);
    TEST_ASSERT_LESS_THAN_UINT64(10000, elapsed);
}

void test_time_sleep_ms(void) {
    time_ms_t start = time_now_ms();
    time_sleep_ms(10);
    time_ms_t elapsed = time_since_ms(start);
    
    TEST_ASSERT_GREATER_THAN_UINT32(8, elapsed);
    TEST_ASSERT_LESS_THAN_UINT32(20, elapsed);
}

static void timer_callback_one_shot(void* arg) {
    *(int*)arg = 1;
}

void test_timer_create_start_stop(void) {
    timer_t timer;
    int called = 0;
    
    timer_create(&timer, 10000, timer_callback_one_shot, &called, false);
    
    timer_start(&timer);
    TEST_ASSERT_TRUE(timer.active);
    
    timer_stop(&timer);
    TEST_ASSERT_FALSE(timer.active);
    
    timer_delete(&timer);
    TEST_ASSERT_NULL(timer.callback);
}

static void timer_callback_periodic(void* arg) {
    (*(int*)arg)++;
}

void test_timer_periodic(void) {
    timer_t timer;
    int count = 0;
    
    timer_create(&timer, 1000, timer_callback_periodic, &count, true);
    
    timer_start(&timer);
    
    for (int i = 0; i < 5; i++) {
        timers_process();
        time_sleep_ms(2);
    }
    
    TEST_ASSERT_GREATER_THAN(0, count);
    
    timer_stop(&timer);
    timer_delete(&timer);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_time_now_us_should_increase);
    RUN_TEST(test_time_now_ms_should_increase);
    RUN_TEST(test_time_since_us);
    RUN_TEST(test_time_sleep_us);
    RUN_TEST(test_time_sleep_ms);
    RUN_TEST(test_timer_create_start_stop);
    RUN_TEST(test_timer_periodic);
    
    return UNITY_END();
}