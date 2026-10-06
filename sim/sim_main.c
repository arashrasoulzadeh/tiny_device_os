#include "sim_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <fcntl.h>

/* getopt.h was included but never used anywhere in this file (no call to
 * getopt()/getopt_long(), no optarg/optind/struct option reference) -
 * removed rather than ported, since there's nothing to port.
 *
 * write()/close()/open()/STDERR_FILENO below (crash_write()) are POSIX,
 * from <unistd.h> - MSVC has no such header. The crash handler is meant
 * to work on Windows too (see README: "Windows CI still gets the signal
 * logged, just without frames"), so this maps to the raw CRT equivalents
 * MSVC does provide via <io.h>, rather than switching to buffered stdio -
 * crash_write() deliberately avoids that (see its own comment) since it
 * runs inside a signal handler after memory may already be corrupt. */
#if defined(_WIN32)
#include <io.h>
#define write(fd, buf, n) _write((fd), (buf), (unsigned)(n))
#define close(fd) _close(fd)
#define open(path, flags, mode) _open((path), (flags), (mode))
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#else
#include <unistd.h>
#endif

#include "scheduler.h"
#include "stdapps_register.h"
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
#include "app_framework.h"
#include "status.h"
#include "littlefs_vfs.h"
#include "config_store.h"
#include "clock_service.h"
#include "sensor_service.h"
#include "ardubot_enabled_apps.h"
#include "input.h"

#if defined(__APPLE__) || defined(__linux__)
#define ARDUBOT_SIM_HAVE_BACKTRACE 1
#include <execinfo.h>
#endif

/* Crash-dump-to-flash (Phase 7): the sim equivalent of a hardware crash
 * dump is a plain host file, since there is no real flash sector to write
 * to and the fault may happen before/during VFS mount. Written directly
 * with write()/async-signal-safe calls only - no malloc, no printf - since
 * this runs inside a signal handler after memory may already be corrupt. */
#define CRASH_LOG_PATH "crash.log"

static void crash_write(const char* s) {
    /* write() is glibc's warn_unused_result - there's nothing useful to
     * do with a failed write from inside a signal handler (can't log,
     * can't retry safely), but a bare (void)write(...) doesn't actually
     * silence that specific attribute on some gcc versions; assigning
     * the result does. */
    int ret = (int)write(STDERR_FILENO, s, strlen(s));
    (void)ret;
    int fd = open(CRASH_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        ret = (int)write(fd, s, strlen(s));
        (void)ret;
        close(fd);
    }
}

static void crash_handler(int sig) {
    char buf[64];
    snprintf(buf, sizeof(buf), "\n=== ArdubotOS sim crash: signal %d ===\n", sig);
    crash_write(buf);

#ifdef ARDUBOT_SIM_HAVE_BACKTRACE
    void* frames[64];
    int n = backtrace(frames, 64);
    int fd = open(CRASH_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        backtrace_symbols_fd(frames, n, fd);
        close(fd);
    }
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
#else
    crash_write("(no backtrace support on this platform)\n");
#endif

    signal(sig, SIG_DFL);
    raise(sig);
}

/* sim_key_to_gpio_cb() and sim_system_key_cb() used to be two separate
 * callbacks (app-keys-to-GPIO, and OS-level system keys) - both
 * superseded by sim_combined_key_cb() below, which does both in one
 * pass and is the one actually registered (sim_video_set_key_callback()
 * further down). Neither standalone version is referenced anywhere
 * anymore - removed as dead duplicates rather than leaving them under
 * -Werror=unused-function. */

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
    (void)pin; (void)level; (void)arg;
    /* Not implemented: HAL GPIO edge detection (hal_gpio_check_edge) is
     * static, so it can't be invoked from here - this registered callback
     * is currently a no-op. sim_gpio_handle_key() (used by the actual
     * active key->GPIO path, sim_combined_key_cb() above) already covers
     * the key-press-to-GPIO case; a real HAL-level edge-triggered GPIO
     * callback still needs wiring up. */
}

void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

static void sim_quit_cb(void* arg) {
    (void)arg;
    g_running = false;
}

/* demo_task_entry() was a scratch task (incremented a counter nobody
 * read, every 100ms) never actually started via task_create() anywhere
 * - removed as dead code under -Werror=unused-function. */

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
#ifndef _WIN32
    signal(SIGBUS, crash_handler);
#endif
    
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

    /* Needed before any app_ui_bind_gesture() call: without this, apps/
     * input.c's tap/long-press/hold thresholds all default to 0 (it's a
     * zero-initialized static struct), so every single key release would
     * classify as an immediate BUTTON_RELEASE/hold regardless of how
     * briefly it was pressed. */
    if (input_init(NULL) != 0) {
        fprintf(stderr, "Failed to initialize input gesture system\n");
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

    /* Mount /flash and open the config store the Settings app (and anything
     * else using vfs_open()/app_config_*) reads and writes through - without
     * this nothing persisted even within a single run, since app_config_*
     * silently no-ops with no store set and /flash didn't exist as a VFS
     * mount point at all. */
    {
        hal_storage_t* flash_storage = hal_storage_open("/dev/flash0", HAL_STORAGE_TYPE_FLASH);
        if (!flash_storage || hal_storage_init(flash_storage) != 0 ||
            littlefs_mount(flash_storage, 0, 4 * 1024 * 1024, 4096, "/flash") != 0) {
            fprintf(stderr, "Failed to mount /flash\n");
        } else {
            config_store_t* cfg_store = config_store_open("/flash/config.dat");
            if (!cfg_store || config_store_init(cfg_store) != 0) {
                fprintf(stderr, "Failed to open config store\n");
            } else {
                app_config_init(cfg_store);
            }
        }
        clock_service_start(time(NULL), 0);
        sensor_service_load_builtin();
    }

    /* Builtin apps are installed only from apps/stdapps_register.c.
     * ESC from the start app still returns to the launcher
     * (app_request_exit() re-focuses APP_KIT_HOME_NAME="launcher"). */
    if (stdapps_install() != 0) {
        fprintf(stderr, "Failed to install builtin apps\n");
        return 1;
    }
    {
        const char* start = stdapps_start_name();
        if (app_start(start) != 0) {
            fprintf(stderr, "Failed to start %s app\n", start);
            return 1;
        }
        printf("%s started (main app)\n", start);
        fflush(stdout);
    }

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