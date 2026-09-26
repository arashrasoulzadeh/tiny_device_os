#include "unity.h"
#include "host_stack.h"
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

void setUp(void) {}
void tearDown(void) {}

void test_host_stack_bootstrap_skipped_on_windows(void) {
    /* Windows host tasks use Fibers in scheduler.c; host_call_on_stack is unused. */
    TEST_PASS();
}

#else

static jmp_buf g_back;
static int g_marker;

static void boot_fn(void* arg) {
    int* out = (int*)arg;
    *out = 42;
    g_marker = 1;
    longjmp(g_back, 1);
}

void setUp(void) {
    g_marker = 0;
}

void tearDown(void) {}

void test_host_call_on_stack_runs_fn_and_longjmps_back(void) {
    size_t size = HOST_STACK_MIN_BYTES;
    void* stack = malloc(size);
    TEST_ASSERT_NOT_NULL(stack);
    memset(stack, 0, size);

    uintptr_t top = ((uintptr_t)stack + size) & ~(uintptr_t)15u;
    int value = 0;

    if (setjmp(g_back) == 0) {
        host_call_on_stack((void*)top, boot_fn, &value);
        TEST_FAIL_MESSAGE("host_call_on_stack returned without longjmp");
    }

    TEST_ASSERT_EQUAL(42, value);
    TEST_ASSERT_EQUAL(1, g_marker);
    free(stack);
}

#endif

int main(void) {
    UNITY_BEGIN();
#if defined(_WIN32)
    RUN_TEST(test_host_stack_bootstrap_skipped_on_windows);
#else
    RUN_TEST(test_host_call_on_stack_runs_fn_and_longjmps_back);
#endif
    return UNITY_END();
}
