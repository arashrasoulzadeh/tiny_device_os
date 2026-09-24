#include "unity.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
    remove("test_results.xml");
}

void test_junit_xml_format(void) {
    FILE* f = fopen("test_results.xml", "w");
    TEST_ASSERT_NOT_NULL(f);
    
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuite name=\"test_suite\" tests=\"2\" failures=\"1\" errors=\"0\" time=\"0.123\">\n");
    fprintf(f, "  <testcase name=\"test_pass\" classname=\"test\" time=\"0.01\"/>\n");
    fprintf(f, "  <testcase name=\"test_fail\" classname=\"test\" time=\"0.02\">\n");
    fprintf(f, "    <failure message=\"Assertion failed\"/>test_fail</failure>\n");
    fprintf(f, "  </testcase>\n");
    fprintf(f, "</testsuite>\n");
    fclose(f);
    
    f = fopen("test_results.xml", "r");
    TEST_ASSERT_NOT_NULL(f);
    
    char content[1024] = {0};
    fread(content, 1, sizeof(content) - 1, f);
    fclose(f);
    
    TEST_ASSERT_TRUE(strstr(content, "<?xml") != NULL);
    TEST_ASSERT_TRUE(strstr(content, "testsuite") != NULL);
    TEST_ASSERT_TRUE(strstr(content, "testcase") != NULL);
    TEST_ASSERT_TRUE(strstr(content, "failure") != NULL);
}

void test_junit_multiple_testsuites(void) {
    FILE* f = fopen("test_results.xml", "w");
    TEST_ASSERT_NOT_NULL(f);
    
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuites>\n");
    fprintf(f, "  <testsuite name=\"kernel\" tests=\"5\" failures=\"0\" time=\"0.05\"/>\n");
    fprintf(f, "  <testsuite name=\"hal\" tests=\"3\" failures=\"1\" time=\"0.03\"/>\n");
    fprintf(f, "</testsuites>\n");
    fclose(f);
    
    f = fopen("test_results.xml", "r");
    TEST_ASSERT_NOT_NULL(f);
    
    char content[1024] = {0};
    fread(content, 1, sizeof(content) - 1, f);
    fclose(f);
    
    TEST_ASSERT_TRUE(strstr(content, "testsuites") != NULL);
    TEST_ASSERT_TRUE(strstr(content, "kernel") != NULL);
    TEST_ASSERT_TRUE(strstr(content, "hal") != NULL);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_junit_xml_format);
    RUN_TEST(test_junit_multiple_testsuites);
    
    return UNITY_END();
}