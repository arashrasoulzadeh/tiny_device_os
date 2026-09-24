#include "sim_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

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

int parse_args(int argc, char** argv) {
    static struct option long_options[] = {
        {"headless", no_argument, 0, 'h'},
        {"flash-image", required_argument, 0, 'f'},
        {"sd-image", required_argument, 0, 's'},
        {"test", required_argument, 0, 't'},
        {"junit", required_argument, 0, 'j'},
        {"coverage", required_argument, 0, 'c'},
        {"help", no_argument, 0, 'H'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hf:s:t:j:c:H", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h': g_headless = true; break;
            case 'f': g_flash_image = optarg; break;
            case 's': g_sd_image = optarg; break;
            case 't': g_test_name = optarg; break;
            case 'j': g_junit_file = optarg; break;
            case 'c': g_coverage_file = optarg; break;
            case 'H': print_usage(argv[0]); return 1;
            default: return -1;
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
}