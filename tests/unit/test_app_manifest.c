#include "unity.h"
#include "app_manifest.h"

void setUp(void) {}
void tearDown(void) {}

void test_counter_identity_comes_from_app_json(void) {
    TEST_ASSERT_EQUAL_STRING("2.0.0", app_manifest_version("counter"));
    TEST_ASSERT_EQUAL_STRING("ArdubotOS", app_manifest_author("counter"));
    TEST_ASSERT_EQUAL_STRING("Dynamic counter with sign-colored gauge",
                             app_manifest_description("counter"));
    TEST_ASSERT_EQUAL_STRING("COUNTER", app_manifest_title("counter"));
    TEST_ASSERT_EQUAL_STRING("Up:+  Sel:-  hold:back", app_manifest_help("counter"));
}

void test_unknown_name_uses_the_builtin_defaults(void) {
    TEST_ASSERT_EQUAL_STRING("1.0.0", app_manifest_version("missing"));
    TEST_ASSERT_EQUAL_STRING("ArdubotOS", app_manifest_author(NULL));
    TEST_ASSERT_EQUAL_STRING("missing", app_manifest_description("missing"));
    TEST_ASSERT_EQUAL_STRING("", app_manifest_description(NULL));
    TEST_ASSERT_EQUAL_STRING("missing", app_manifest_title("missing"));
    TEST_ASSERT_EQUAL_STRING("", app_manifest_title(NULL));
    TEST_ASSERT_EQUAL_STRING("", app_manifest_help("missing"));
    TEST_ASSERT_EQUAL_STRING("", app_manifest_help(NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_counter_identity_comes_from_app_json);
    RUN_TEST(test_unknown_name_uses_the_builtin_defaults);
    return UNITY_END();
}
