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
#include "sim_audio.h"
#include "ssd1306_model.h"
#include "bmp280_model.h"
#include "sim_gpio.h"
#include "app_kit.h"
#include "status.h"

// Key callback that forwards app keys to GPIO (ignores system keys)
static void sim_key_to_gpio_cb(sim_key_t key, bool pressed, void* arg) {
    (void)arg;
    // Only forward app-class keys to GPIO, system keys are handled by OS
    if (!sim_key_is_system(key)) {
        sim_gpio_handle_key(key, pressed);
    }
}

// System key handler for OS-level keys
static void sim_system_key_cb(sim_key_t key, bool pressed, void* arg) {
    (void)arg;
    if (!pressed) return;  // Only handle key press, not release
    
    if (sim_key_is_system(key)) {
        switch (key) {
            case SIM_KEY_SYS_NEXT_APP:
                printf("System: Switching to next app\n");
                fflush(stdout);
                // TODO: os_app_switch_next();
                break;
            case SIM_KEY_SYS_ESCAPE:
                printf("System: Escape - quit current app\n");
                fflush(stdout);
                // TODO: os_app_quit_current();
                break;
            case SIM_KEY_SYS_MENU:
                printf("System: Show system menu\n");
                fflush(stdout);
                // TODO: os_sys_show_menu();
                break;
            default:
                break;
        }
    }
}

// Combined key callback that handles both system and app keys
static void sim_combined_key_cb(sim_key_t key, bool pressed, void* arg) {
    (void)arg;
    // First, handle system keys
    if (sim_key_is_system(key)) {
        if (pressed) {
            switch (key) {
                case SIM_KEY_SYS_NEXT_APP:
                    printf("System: Switching to next app\n");
                    fflush(stdout);
                    break;
                case SIM_KEY_SYS_ESCAPE:
                    printf("System: Escape - quit current app\n");
                    fflush(stdout);
                    break;
                case SIM_KEY_SYS_MENU:
                    printf("System: Show system menu\n");
                    fflush(stdout);
                    break;
                default:
                break;
            }
        }
        return;
    }
    // Then handle app keys (forward to GPIO)
    sim_gpio_handle_key(key, pressed);
}

// GPIO callback for HAL GPIO level changes
static void sim_gpio_callback(int pin, bool level, void* arg) {
    (void)arg;
    // Forward to HAL GPIO edge detection via global callback
    // The global g_callback is set by sim_gpio_register_hal_gpio_with_trigger
    // But we need to manually invoke the edge detection for pins with registered HAL callbacks
    for (int i = 0; i < 64; i++) {
        extern bool sim_gpio_read(int pin);  // This won't work directly
    }
    // Actually, we need to call the internal edge check function
    // Since hal_gpio_check_edge is static, we can't call it directly
    // Instead, let's just invoke any registered HAL GPIO callback directly
    // The key is that sim_gpio_handle_key already calls g_callback
}

void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

static void sim_quit_cb(void* arg) {
    (void)arg;
    g_running = false;
}

static int g_demo_counter = 0;

static void demo_task_entry(void* arg) {
    (void)arg;
    while (1) {
        g_demo_counter++;
        task_sleep(100); // Sleep 100 ticks (100ms)
    }
}

// External builtin app manifests (from APP_DEFINE)
extern app_manifest_t* counter_app_manifest;
extern app_manifest_t* launcher_app_manifest;
extern app_manifest_t* info_app_manifest;
extern app_manifest_t* stopwatch_app_manifest;
extern app_manifest_t* pong_app_manifest;
extern app_manifest_t* settings_app_manifest;
extern app_manifest_t* fileman_app_manifest;
extern app_manifest_t* shell_app_manifest;
extern app_manifest_t* demo_app_manifest;

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
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
    
    if (sim_gpio_init() != 0) {
        fprintf(stderr, "Failed to initialize sim gpio\n");
        return 1;
    }
    
    // Set GPIO callback to handle pin level changes from key events
    sim_gpio_set_callback(sim_gpio_callback, NULL);
    
    if (sim_storage_init(g_flash_image, g_sd_image) != 0) {
        fprintf(stderr, "Failed to initialize storage\n");
    }
    
    if (!g_headless) {
        if (sim_video_init(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT, "ArdubotOS Simulator") != 0) {
            fprintf(stderr, "Failed to initialize video\n");
        }
        
        if (sim_audio_init(44100, 2, 512) != 0) {
            fprintf(stderr, "Failed to initialize audio\n");
        }
        
        sim_video_set_key_callback(sim_combined_key_cb, NULL);
        sim_video_set_quit_callback(sim_quit_cb, NULL);
    }
    hal_audio_config_t audio_config = {
        .sample_rate = 44100,
        .channels = 2,
        .format = HAL_AUDIO_FORMAT_PCM_S16_LE,
        .buffer_frames = 512,
        .period_frames = 256,
        .output = true,
        .input = false
    };
    hal_audio_t* hal_audio = hal_audio_open("/dev/audio0", &audio_config);
    if (hal_audio) {
        hal_audio_start(hal_audio);
        sim_audio_set_hal_audio(hal_audio);
        sim_audio_start();
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
    
    // Initialize app system
    if (app_init() != 0) {
        fprintf(stderr, "Failed to initialize app system\n");
        return 1;
    }
    /* Simulated pack level (USB host ≈ full). */
    app_status_set_battery_percent(92);

    /* Install builtins, then start the home/launcher app.
     * Main app is selected here via app_start("launcher"). */
    if (app_install_manifest(counter_app_manifest, "counter") != 0) {
        fprintf(stderr, "Failed to install counter app\n");
        return 1;
    }
    if (app_install_manifest(info_app_manifest, "info") != 0) {
        fprintf(stderr, "Failed to install info app\n");
        return 1;
    }
    if (app_install_manifest(stopwatch_app_manifest, "stopwatch") != 0) {
        fprintf(stderr, "Failed to install stopwatch app\n");
        return 1;
    }
    if (app_install_manifest(pong_app_manifest, "pong") != 0) {
        fprintf(stderr, "Failed to install pong app\n");
        return 1;
    }
    if (app_install_manifest(settings_app_manifest, "settings") != 0) {
        fprintf(stderr, "Failed to install settings app\n");
        return 1;
    }
    if (app_install_manifest(fileman_app_manifest, "fileman") != 0) {
        fprintf(stderr, "Failed to install fileman app\n");
        return 1;
    }
    if (app_install_manifest(shell_app_manifest, "shell") != 0) {
        fprintf(stderr, "Failed to install shell app\n");
        return 1;
    }
    if (app_install_manifest(demo_app_manifest, "demo") != 0) {
        fprintf(stderr, "Failed to install demo app\n");
        return 1;
    }
    if (app_install_manifest(launcher_app_manifest, "launcher") != 0) {
        fprintf(stderr, "Failed to install launcher app\n");
        return 1;
    }

    /* Snapshot launchable apps once — launcher menu reads this, no per-visit refresh. */
    if (app_kit_catalog_build("launcher") < 0) {
        fprintf(stderr, "Failed to build app catalog\n");
        return 1;
    }

    if (app_start("launcher") != 0) {
        fprintf(stderr, "Failed to start launcher app\n");
        return 1;
    }

    printf("Launcher started (main app)\n");
    fflush(stdout);

    /* One scheduler tick == 1 ms of wall time. SDL frames often take longer
     * than 1 ms, so catch up multiple ticks per loop from sim_time. */
    uint32_t last_tick_ms = sim_time_now_ms();

    while (g_running) {
        sim_time_update();
        sim_video_poll_events();
        
        // Ensure window keeps focus on macOS
        if (!g_headless) {
            sim_video_ensure_focus();
        }

        scheduler_step();

        if (!g_headless) {
            ssd1306_model_render();
            sim_video_render();
        }

        {
            uint32_t now_ms = sim_time_now_ms();
            uint32_t dt = now_ms - last_tick_ms;
            last_tick_ms = now_ms;
            if (dt == 0) {
                dt = 1;
            } else if (dt > 50) {
                dt = 50; /* clamp after pauses / breakpoints */
            }
            for (uint32_t t = 0; t < dt; t++) {
                scheduler_tick();
            }
            /* Run any tasks that woke during the catch-up. */
            scheduler_step();
        }
        timers_process();
        sim_time_sleep_ms(1);
    }
    
    sim_video_cleanup();
    sim_audio_cleanup();
    sim_storage_cleanup();
    
    return 0;
}