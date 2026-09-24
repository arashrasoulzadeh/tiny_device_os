#include "sim_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include "scheduler.h"
#include "os_time.h"
#include "hal_gpio.h"
#include "hal_display.h"
#include "hal_audio.h"
#include "hal_storage.h"
#include "sim_video.h"
#include "sim_time.h"
#include "sim_storage.h"
#include "ssd1306_model.h"
#include "bmp280_model.h"

void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (parse_args(argc, argv) != 0) {
        return 1;
    }
    
    if (g_headless) {
        printf("ArdubotOS Simulator (headless mode)\n");
    } else {
        printf("ArdubotOS Simulator (interactive mode)\n");
    }
    
    if (scheduler_init() != 0) {
        fprintf(stderr, "Failed to initialize scheduler\n");
        return 1;
    }
    
    if (sim_time_init() != 0) {
        fprintf(stderr, "Failed to initialize sim time\n");
        return 1;
    }
    
    if (sim_storage_init(g_flash_image, g_sd_image) != 0) {
        fprintf(stderr, "Failed to initialize storage\n");
    }
    
    if (!g_headless) {
        if (sim_video_init(320, 240, "ArdubotOS Simulator") != 0) {
            fprintf(stderr, "Failed to initialize video\n");
        }
    }
    
    ssd1306_model_register();
    bmp280_model_register();
    
    if (g_test_name || g_headless) {
        run_tests();
        
        if (g_headless) {
            printf("Headless test run complete, exiting\n");
            return 0;
        }
    }
    
    if (scheduler_start() != 0) {
        fprintf(stderr, "Failed to start scheduler\n");
        return 1;
    }
    
    while (g_running) {
        sim_time_update();
        sim_video_poll_events();
        
        if (!g_headless) {
            sim_video_render();
        }
        
        scheduler_tick();
        timers_process();
        
        sim_time_sleep_ms(1);
    }
    
    sim_video_cleanup();
    sim_storage_cleanup();
    
    return 0;
}