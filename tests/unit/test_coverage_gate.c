#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_coverage_check_script_exists(void) {
    FILE* f = fopen("scripts/check_coverage.cmake", "r");
    TEST_ASSERT_NOT_NULL(f);
    if (f) fclose(f);
}

void test_coverage_gate_80_line_70_branch(void) {
    FILE* f = fopen("coverage_test.info", "w");
    TEST_ASSERT_NOT_NULL(f);
    
    fprintf(f, "TN:\n");
    fprintf(f, "SF:test.c\n");
    fprintf(f, "DA:1,1\n");
    fprintf(f, "DA:2,1\n");
    fprintf(f, "DA:3,1\n");
    fprintf(f, "DA:4,1\n");
    fprintf(f, "DA:5,0\n");
    fprintf(f, "DA:6,0\n");
    fprintf(f, "DA:7,0\n");
    fprintf(f, "DA:8,0\n");
    fprintf(f, "DA:9,0\n");
    fprintf(f, "DA:10,0\n");
    fprintf(f, "end_of_record\n");
    fclose(f);
    
    int ret = system("lcov --summary coverage_test.info 2>&1 | grep -q 'lines......: 40.0%'");
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    remove("coverage_test.info");
}

void test_coverage_passes_at_80_percent(void) {
    FILE* f = fopen("coverage_test.info", "w");
    TEST_ASSERT_NOT_NULL(f);
    
    fprintf(f, "TN:\n");
    fprintf(f, "SF:test.c\n");
    for (int i = 1; i <= 100; i++) {
        if (i <= 85) fprintf(f, "DA:%d,1\n", i);
        else fprintf(f, "DA:%d,0\n", i);
    }
    fprintf(f, "end_of_record\n");
    fclose(f);
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "lcov --summary coverage_test.info 2>&1 | grep -q 'lines......: 85.0%%'");
    int ret = system(cmd);
    TEST_ASSERT_EQUAL(0, ret);
    
    remove("coverage_test.info");
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_coverage_check_script_exists);
    RUN_TEST(test_coverage_gate_80_line_70_branch);
    RUN_TEST(test_coverage_passes_at_80_percent);
    
    return UNITY_END();
}