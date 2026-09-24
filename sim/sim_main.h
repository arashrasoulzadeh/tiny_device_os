#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool g_running;
extern bool g_headless;
extern const char* g_flash_image;
extern const char* g_sd_image;
extern const char* g_test_name;
extern const char* g_junit_file;
extern const char* g_coverage_file;

int parse_args(int argc, char** argv);
void run_tests(void);

#ifdef __cplusplus
}
#endif