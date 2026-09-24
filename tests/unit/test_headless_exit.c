#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

extern bool g_running;
extern bool g_headless;
extern const char* g_flash_image;
extern const char* g_sd_image;
extern const char* g_test_name;
extern const char* g_junit_file;
extern const char* g_coverage_file;

extern int parse_args(int argc, char** argv);
extern void run_tests(void);

void setUp(void) {
    g_running = true;
    g_headless = true;
    g_flash_image = "flash.img";
    g_sd_image = "sd.img";
    g_test_name = NULL;
    g_junit_file = NULL;
    g_coverage_file = NULL;
}

void tearDown(void) {
    remove("results.xml");
    remove("cov.info");
}

void test_headless_should_exit_after_tests(void) {
    const char* test_argv[] = {"ardubot-sim", "--headless", "--test=all"};
    parse_args(3, (char**)test_argv);
    
    run_tests();
    
    TEST_ASSERT_FALSE(g_running);
}

void test_headless_with_junit(void) {
    const char* test_argv[] = {"ardubot-sim", "--headless", "--junit=results.xml"};
    parse_args(3, (char**)test_argv);
    
    run_tests();
    
    FILE* f = fopen("results.xml", "r");
    TEST_ASSERT_NOT_NULL(f);
    if (f) {
        char buf[256];
        char* result = fgets(buf, sizeof(buf), f);
        (void)result;
        TEST_ASSERT_TRUE(strstr(buf, "<?xml") != NULL);
        fclose(f);
    }
}

void test_headless_with_coverage(void) {
    const char* test_argv[] = {"ardubot-sim", "--headless", "--coverage=cov.info"};
    parse_args(3, (char**)test_argv);
    
    run_tests();
    
    FILE* f = fopen("cov.info", "r");
    TEST_ASSERT_NOT_NULL(f);
    if (f) {
        char buf[256];
        char* result = fgets(buf, sizeof(buf), f);
        (void)result;
        TEST_ASSERT_TRUE(strstr(buf, "TN:") != NULL);
        fclose(f);
    }
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_headless_should_exit_after_tests);
    RUN_TEST(test_headless_with_junit);
    RUN_TEST(test_headless_with_coverage);
    
    return UNITY_END();
}