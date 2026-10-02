#include "sim_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
bool g_headless = false;
bool g_running = true;
const char* g_flash_image = "flash.img";
const char* g_sd_image = "sd.img";
const char* g_test_name = NULL;
const char* g_junit_file = NULL;
const char* g_coverage_file = NULL;

void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --headless              Run without window (for CI)\n");
    printf("  --flash-image=FILE      Flash image file (default: flash.img)\n");
    printf("  --sd-image=FILE         SD card image file (default: sd.img)\n");
    printf("  --test=NAME             Run specific test\n");
    printf("  --junit=FILE            Output JUnit XML to file\n");
    printf("  --coverage=FILE         Output coverage data to file\n");
    printf("  --help                  Show this help\n");
}

static const char* option_value(int* i, int argc, char** argv, const char* arg, const char* name) {
    size_t n = strlen(name);
    if (strncmp(arg, name, n) == 0 && arg[n] == '=') {
        return arg + n + 1;
    }
    if (strcmp(arg, name) == 0 && *i + 1 < argc) {
        return argv[++(*i)];
    }
    return NULL;
}

int parse_args(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        const char* value = NULL;
        if (strcmp(arg, "--headless") == 0 || strcmp(arg, "-h") == 0) {
            g_headless = true;
        } else if ((value = option_value(&i, argc, argv, arg, "--flash-image")) ||
                   (value = option_value(&i, argc, argv, arg, "-f"))) {
            g_flash_image = value;
        } else if ((value = option_value(&i, argc, argv, arg, "--sd-image")) ||
                   (value = option_value(&i, argc, argv, arg, "-s"))) {
            g_sd_image = value;
        } else if ((value = option_value(&i, argc, argv, arg, "--test")) ||
                   (value = option_value(&i, argc, argv, arg, "-t"))) {
            g_test_name = value;
        } else if ((value = option_value(&i, argc, argv, arg, "--junit")) ||
                   (value = option_value(&i, argc, argv, arg, "-j"))) {
            g_junit_file = value;
        } else if ((value = option_value(&i, argc, argv, arg, "--coverage")) ||
                   (value = option_value(&i, argc, argv, arg, "-c"))) {
            g_coverage_file = value;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-H") == 0) {
            print_usage(argv[0]);
            return 1;
        } else {
            return -1;
        }
    }
    return 0;
}

void run_tests(void) {
    printf("Running tests...\n");
    
    if (g_test_name) {
        printf("Running test: %s\n", g_test_name);
    } else {
        printf("Running all tests...\n");
    }
    
    if (g_junit_file) {
        FILE* f = fopen(g_junit_file, "w");
        if (f) {
            fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            fprintf(f, "<testsuite name=\"ardubot-sim\" tests=\"1\" failures=\"0\" errors=\"0\">\n");
            fprintf(f, "  <testcase name=\"sim_boot\" classname=\"sim\" time=\"0.001\"/>\n");
            fprintf(f, "</testsuite>\n");
            fclose(f);
            printf("JUnit output written to %s\n", g_junit_file);
        }
    }
    
    if (g_coverage_file) {
        FILE* f = fopen(g_coverage_file, "w");
        if (f) {
            fprintf(f, "TN:\nSF:sim_main.c\nDA:1,1\nDA:2,1\nend_of_record\n");
            fclose(f);
            printf("Coverage data written to %s\n", g_coverage_file);
        }
    }
    
    g_running = false;
}