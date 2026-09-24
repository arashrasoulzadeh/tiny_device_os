#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_tdd_check_script_exists(void) {
    FILE* f = fopen("scripts/tdd_check.py", "r");
    TEST_ASSERT_NOT_NULL(f);
    if (f) fclose(f);
}

void test_tdd_check_detects_missing_test(void) {
    FILE* f = popen("python3 scripts/tdd_check.py --staged 2>&1", "r");
    TEST_ASSERT_NOT_NULL(f);
    
    char output[1024] = {0};
    fread(output, 1, sizeof(output) - 1, f);
    pclose(f);
    
    TEST_ASSERT_TRUE(strstr(output, "TDD") != NULL || strstr(output, "test") != NULL);
}

void test_tdd_check_passes_with_test(void) {
    system("git add tests/unit/test_kernel_boot.c 2>/dev/null");
    
    FILE* f = popen("python3 scripts/tdd_check.py --staged 2>&1", "r");
    TEST_ASSERT_NOT_NULL(f);
    
    char output[1024] = {0};
    fread(output, 1, sizeof(output) - 1, f);
    int ret = pclose(f);
    
    TEST_ASSERT_EQUAL(0, ret);
    
    system("git reset HEAD tests/unit/test_kernel_boot.c 2>/dev/null");
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_tdd_check_script_exists);
    RUN_TEST(test_tdd_check_detects_missing_test);
    RUN_TEST(test_tdd_check_passes_with_test);
    
    return UNITY_END();
}