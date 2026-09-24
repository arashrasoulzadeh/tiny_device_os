#include "unity.h"
#include "sim_main.h"

extern bool g_headless;
extern const char* g_flash_image;
extern const char* g_sd_image;
extern const char* g_test_name;
extern const char* g_junit_file;
extern const char* g_coverage_file;

extern int parse_args(int argc, char** argv);

void setUp(void) {
    g_headless = false;
    g_flash_image = "flash.img";
    g_sd_image = "sd.img";
    g_test_name = NULL;
    g_junit_file = NULL;
    g_coverage_file = NULL;
}

void tearDown(void) {
}

int run_parse_args(int argc, char** argv) {
    return parse_args(argc, argv);
}

void test_parse_headless(void) {
    char* argv[] = {"ardubot-sim", "--headless"};
    int ret = run_parse_args(2, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_TRUE(g_headless);
}

void test_parse_flash_image(void) {
    char* argv[] = {"ardubot-sim", "--flash-image=custom.img"};
    int ret = run_parse_args(2, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL_STRING("custom.img", g_flash_image);
}

void test_parse_sd_image(void) {
    char* argv[] = {"ardubot-sim", "--sd-image=sdcard.img"};
    int ret = run_parse_args(2, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL_STRING("sdcard.img", g_sd_image);
}

void test_parse_test_name(void) {
    char* argv[] = {"ardubot-sim", "--test=test_scheduler"};
    int ret = run_parse_args(2, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL_STRING("test_scheduler", g_test_name);
}

void test_parse_junit(void) {
    char* argv[] = {"ardubot-sim", "--junit=results.xml"};
    int ret = run_parse_args(2, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL_STRING("results.xml", g_junit_file);
}

void test_parse_coverage(void) {
    char* argv[] = {"ardubot-sim", "--coverage=cov.info"};
    int ret = run_parse_args(2, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL_STRING("cov.info", g_coverage_file);
}

void test_parse_multiple_args(void) {
    char* argv[] = {"ardubot-sim", "--headless", "--flash-image=f.img", "--test=all", "--junit=j.xml"};
    int ret = run_parse_args(5, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_TRUE(g_headless);
    TEST_ASSERT_EQUAL_STRING("f.img", g_flash_image);
    TEST_ASSERT_EQUAL_STRING("all", g_test_name);
    TEST_ASSERT_EQUAL_STRING("j.xml", g_junit_file);
}

void test_parse_short_options(void) {
    char* argv[] = {"ardubot-sim", "-h", "-f", "f.img", "-s", "s.img"};
    int ret = run_parse_args(6, argv);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_TRUE(g_headless);
    TEST_ASSERT_EQUAL_STRING("f.img", g_flash_image);
    TEST_ASSERT_EQUAL_STRING("s.img", g_sd_image);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_parse_headless);
    RUN_TEST(test_parse_flash_image);
    RUN_TEST(test_parse_sd_image);
    RUN_TEST(test_parse_test_name);
    RUN_TEST(test_parse_junit);
    RUN_TEST(test_parse_coverage);
    RUN_TEST(test_parse_multiple_args);
    RUN_TEST(test_parse_short_options);
    
    return UNITY_END();
}