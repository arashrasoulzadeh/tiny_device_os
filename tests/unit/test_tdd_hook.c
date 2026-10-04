#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* popen/pclose are POSIX; MSVC's CRT has the same functionality under
 * underscore-prefixed names (_popen/_pclose) instead. */
#if defined(_MSC_VER)
#define popen _popen
#define pclose _pclose
#endif

/* All three tests here used relative paths ("scripts/tdd_check.py") and
 * relative-cwd git commands, assuming the test runs with the repo root
 * as its current directory. ctest doesn't guarantee that - it runs from
 * the build directory - so every one of these failed before actually
 * exercising tdd_check.py at all. Fixed the same way
 * test_coverage_gate.c already does it: REPO_ROOT_DIR is the repo root,
 * injected by tests/unit/CMakeLists.txt via CMAKE_SOURCE_DIR. */

void setUp(void) {
}

void tearDown(void) {
}

void test_tdd_check_script_exists(void) {
    FILE* f = fopen(REPO_ROOT_DIR "/scripts/tdd_check.py", "r");
    TEST_ASSERT_NOT_NULL(f);
    if (f) fclose(f);
}

void test_tdd_check_detects_missing_test(void) {
    /* tdd_check.py --staged only has anything to say about whatever is
     * actually staged in the real git index right now - this used to
     * just run it against ambient repo state (whatever the person
     * running the suite happened to have staged, often nothing), so it
     * passed or failed depending on an accident of when you ran it
     * rather than what it's meant to check. Stages a real, throwaway
     * implementation file with no corresponding test to force the
     * "missing test" path for real, then cleans up both the git index
     * and the file itself - mirroring test_tdd_check_passes_with_test's
     * own add/reset pattern below. */
    char path_buf[256];
    snprintf(path_buf, sizeof(path_buf), "%s/apps/__tdd_check_scratch.c", REPO_ROOT_DIR);
    FILE* scratch = fopen(path_buf, "w");
    TEST_ASSERT_NOT_NULL(scratch);
    fprintf(scratch, "/* scratch file for test_tdd_check_detects_missing_test - no test exists for this */\nint __tdd_check_scratch(void) { return 0; }\n");
    fclose(scratch);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cd %s && git add apps/__tdd_check_scratch.c 2>/dev/null", REPO_ROOT_DIR);
    int rc = system(cmd);
    (void)rc;

    snprintf(cmd, sizeof(cmd), "cd %s && python3 scripts/tdd_check.py --staged 2>&1", REPO_ROOT_DIR);
    FILE* f = popen(cmd, "r");
    TEST_ASSERT_NOT_NULL(f);

    char output[1024] = {0};
    size_t n = fread(output, 1, sizeof(output) - 1, f);
    (void)n;
    pclose(f);

    snprintf(cmd, sizeof(cmd), "cd %s && git reset HEAD apps/__tdd_check_scratch.c >/dev/null 2>&1", REPO_ROOT_DIR);
    rc = system(cmd);
    (void)rc;
    remove(path_buf);

    TEST_ASSERT_TRUE(strstr(output, "TDD") != NULL || strstr(output, "test") != NULL);
}

void test_tdd_check_passes_with_test(void) {
    char cmd[512];

    snprintf(cmd, sizeof(cmd), "cd %s && git add tests/unit/test_kernel_boot.c 2>/dev/null", REPO_ROOT_DIR);
    int rc = system(cmd);
    (void)rc;

    snprintf(cmd, sizeof(cmd), "cd %s && python3 scripts/tdd_check.py --staged 2>&1", REPO_ROOT_DIR);
    FILE* f = popen(cmd, "r");
    TEST_ASSERT_NOT_NULL(f);

    char output[1024] = {0};
    size_t n = fread(output, 1, sizeof(output) - 1, f);
    (void)n;
    int ret = pclose(f);

    TEST_ASSERT_EQUAL(0, ret);

    snprintf(cmd, sizeof(cmd), "cd %s && git reset HEAD tests/unit/test_kernel_boot.c 2>/dev/null", REPO_ROOT_DIR);
    rc = system(cmd);
    (void)rc;
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tdd_check_script_exists);
    RUN_TEST(test_tdd_check_detects_missing_test);
    RUN_TEST(test_tdd_check_passes_with_test);

    return UNITY_END();
}
