#include "unity.h"
#include "syscall.h"
#include <string.h>

/* syscall.c/syscall.h existed with zero tests and zero callers. Exercises
 * the syscall table (register/invoke/unregister) and the capability bitset
 * (capability_set_*, app_context_*) for the first time. */

void setUp(void) {
    syscall_init();
}

void tearDown(void) {
    syscall_deinit();
}

static int handler_echo(uint32_t* args, uint32_t* ret) {
    *ret = args[0] * 2;
    return 0;
}

void test_invoke_before_register_fails(void) {
    uint32_t args[SYSCALL_MAX_ARGS] = {5};
    uint32_t ret = 0;
    TEST_ASSERT_EQUAL(-1, syscall_invoke(SYS_GET_TICKS, args, &ret));
}

void test_register_then_invoke_calls_handler(void) {
    syscall_entry_t entry = {
        .num = SYS_GET_TICKS,
        .handler = handler_echo,
        .required_cap = CAP_GPIO_READ,
        .name = "test_echo",
    };
    TEST_ASSERT_EQUAL(0, syscall_register(&entry));

    uint32_t args[SYSCALL_MAX_ARGS] = {21};
    uint32_t ret = 0;
    TEST_ASSERT_EQUAL(0, syscall_invoke(SYS_GET_TICKS, args, &ret));
    TEST_ASSERT_EQUAL(42, ret);
}

void test_unregister_stops_invocation(void) {
    syscall_entry_t entry = {
        .num = SYS_GET_TICKS, .handler = handler_echo, .name = "test_echo",
    };
    TEST_ASSERT_EQUAL(0, syscall_register(&entry));
    TEST_ASSERT_EQUAL(0, syscall_unregister(SYS_GET_TICKS));

    uint32_t args[SYSCALL_MAX_ARGS] = {1};
    uint32_t ret = 0;
    TEST_ASSERT_EQUAL(-1, syscall_invoke(SYS_GET_TICKS, args, &ret));
}

void test_register_rejects_out_of_range_num(void) {
    syscall_entry_t entry = {.num = SYS_MAX, .handler = handler_echo, .name = "x"};
    TEST_ASSERT_EQUAL(-1, syscall_register(&entry));
}

void test_register_rejects_null_handler(void) {
    syscall_entry_t entry = {.num = SYS_YIELD, .handler = NULL, .name = "x"};
    TEST_ASSERT_EQUAL(-1, syscall_register(&entry));
}

void test_capability_set_add_has_remove(void) {
    capability_set_t* set = capability_set_create();
    TEST_ASSERT_NOT_NULL(set);

    TEST_ASSERT_FALSE(capability_set_has(set, CAP_GPIO_WRITE));
    capability_set_add(set, CAP_GPIO_WRITE);
    TEST_ASSERT_TRUE(capability_set_has(set, CAP_GPIO_WRITE));

    // A distinct capability must stay unaffected - this is exactly the
    // bug that existed when CAP_* were pre-shifted masks double-shifted
    // again by add/has (see the comment above the CAP_* defines).
    TEST_ASSERT_FALSE(capability_set_has(set, CAP_GPIO_READ));
    TEST_ASSERT_FALSE(capability_set_has(set, CAP_I2C_ACCESS));

    capability_set_remove(set, CAP_GPIO_WRITE);
    TEST_ASSERT_FALSE(capability_set_has(set, CAP_GPIO_WRITE));

    capability_set_destroy(set);
}

void test_capability_set_clear_removes_all(void) {
    capability_set_t* set = capability_set_create();
    capability_set_add(set, CAP_GPIO_READ);
    capability_set_add(set, CAP_NET_ACCESS);
    capability_set_clear(set);
    TEST_ASSERT_FALSE(capability_set_has(set, CAP_GPIO_READ));
    TEST_ASSERT_FALSE(capability_set_has(set, CAP_NET_ACCESS));
    capability_set_destroy(set);
}

void test_capability_set_copy_is_independent(void) {
    capability_set_t* src = capability_set_create();
    capability_set_t* dst = capability_set_create();
    capability_set_add(src, CAP_FS_ACCESS);

    capability_set_copy(dst, src);
    TEST_ASSERT_TRUE(capability_set_has(dst, CAP_FS_ACCESS));

    capability_set_add(dst, CAP_WIFI_ACCESS);
    TEST_ASSERT_FALSE(capability_set_has(src, CAP_WIFI_ACCESS));

    capability_set_destroy(src);
    capability_set_destroy(dst);
}

void test_app_context_check_cap_reflects_granted_set(void) {
    capability_set_t caps;
    memset(&caps, 0, sizeof(caps));
    capability_set_add(&caps, CAP_DISPLAY_ACCESS);

    app_context_t* ctx = app_context_create(1, &caps);
    TEST_ASSERT_NOT_NULL(ctx);

    TEST_ASSERT_EQUAL(0, app_context_check_cap(ctx, CAP_DISPLAY_ACCESS));
    TEST_ASSERT_EQUAL(-1, app_context_check_cap(ctx, CAP_NET_ACCESS));

    app_context_destroy(ctx);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_invoke_before_register_fails);
    RUN_TEST(test_register_then_invoke_calls_handler);
    RUN_TEST(test_unregister_stops_invocation);
    RUN_TEST(test_register_rejects_out_of_range_num);
    RUN_TEST(test_register_rejects_null_handler);
    RUN_TEST(test_capability_set_add_has_remove);
    RUN_TEST(test_capability_set_clear_removes_all);
    RUN_TEST(test_capability_set_copy_is_independent);
    RUN_TEST(test_app_context_check_cap_reflects_granted_set);
    return UNITY_END();
}
