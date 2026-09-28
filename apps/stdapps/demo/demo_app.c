#include "app_kit.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_adc.h"
#include "os_time.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// --- Demo State ---
typedef enum {
    DEMO_MENU,
    DEMO_GPIO,
    DEMO_I2C,
    DEMO_SPI,
    DEMO_ADC,
} demo_mode_t;

static demo_mode_t g_mode = DEMO_MENU;
static int g_selected = 0;
static app_timer_t g_timer;

// Menu items
static const char* g_menu_items[] = {
    "GPIO Test",
    "I2C Scan",
    "SPI Loopback",
    "ADC Read",
    "Exit",
};
#define DEMO_MENU_COUNT 5

// --- Button Handlers ---
static void on_up(app_ctx_t* app, void* user) {
    (void)user;
    if (g_mode == DEMO_MENU) {
        g_selected = (g_selected > 0) ? g_selected - 1 : DEMO_MENU_COUNT - 1;
        app_mark_dirty(app);
    }
}

static void on_down(app_ctx_t* app, void* user) {
    (void)user;
    if (g_mode == DEMO_MENU) {
        g_selected = (g_selected + 1) % DEMO_MENU_COUNT;
        app_mark_dirty(app);
    }
}

static void on_select(app_ctx_t* app, void* user) {
    (void)user;
    switch (g_selected) {
        case 0: g_mode = DEMO_GPIO; break;
        case 1: g_mode = DEMO_I2C; break;
        case 2: g_mode = DEMO_SPI; break;
        case 3: g_mode = DEMO_ADC; break;
        case 4: app_request_exit(app); break;
    }
    app_mark_dirty(app);
}

static void on_back(app_ctx_t* app, void* user) {
    (void)user;
    if (g_mode != DEMO_MENU) {
        g_mode = DEMO_MENU;
        app_mark_dirty(app);
    } else {
        app_request_exit(app);
    }
}

// --- Demo Logic ---
static void gpio_test(void) {
    hal_gpio_t* gpio = hal_gpio_open("/dev/gpio0", HAL_GPIO_MODE_OUTPUT);
    if (!gpio) return;
    
    for (int i = 0; i < 10; i++) {
        hal_gpio_write(gpio, true);
        time_sleep_ms(100);
        hal_gpio_write(gpio, false);
        time_sleep_ms(100);
    }
    hal_gpio_close(gpio);
}

static void i2c_scan(void) {
    hal_i2c_t* i2c = hal_i2c_open("/dev/i2c0", HAL_I2C_SPEED_STANDARD);
    if (!i2c) return;
    
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        if (hal_i2c_read(i2c, addr, &dummy, 1) == 0) {
            printf("Found I2C device at 0x%02X\n", addr);
        }
    }
    hal_i2c_close(i2c);
}

static void spi_loopback(void) {
    hal_spi_config_t cfg = {
        .frequency = 1000000,
        .mode = HAL_SPI_MODE_0,
        .bit_order = HAL_SPI_BIT_ORDER_MSB,
        .bits_per_word = 8,
        .cs_active_high = false,
    };
    hal_spi_t* spi = hal_spi_open("/dev/spi0", &cfg);
    if (!spi) return;
    
    uint8_t tx[] = {0xAA, 0x55, 0x0F, 0xF0};
    uint8_t rx[4];
    hal_spi_transfer(spi, tx, rx, 4);
    hal_spi_close(spi);
}

static void adc_read(void) {
    hal_adc_t* adc = hal_adc_open("/dev/adc0");
    if (!adc) return;
    
    hal_adc_init(adc);
    hal_adc_set_attenuation(adc, ADC_ATTEN_DB_11);
    hal_adc_set_width(adc, ADC_WIDTH_BIT_12);
    
    for (int i = 0; i < 5; i++) {
        uint16_t val;
        if (hal_adc_read(adc, &val) == 0) {
            printf("ADC: %u\n", val);
        }
        time_sleep_ms(500);
    }
    hal_adc_close(adc);
}

// --- Lifecycle ---
static void demo_init(app_ctx_t* app) {
    if (app_display_init(&app->display, "/dev/display0") != 0) {
        APP_ERROR("Display init failed");
        return;
    }
    app_timer_init(&g_timer, 30);

    app_bind_key(app, SIM_KEY_UP, on_up, NULL);
    app_bind_key(app, SIM_KEY_DOWN, on_down, NULL);
    app_bind_key(app, SIM_KEY_ENTER, on_select, NULL);
    app_bind_key(app, SIM_KEY_ESCAPE, on_back, NULL);
    app_bind_back(app);

    APP_INFO("Demo ready - select test");
}

static void demo_frame(app_ctx_t* app) {
    if (!app_screen_begin(app, "HARDWARE DEMO")) return;

    if (g_mode == DEMO_MENU) {
        app_text(app, 0, 0, "=== HARDWARE DEMO ===");
        app_text(app, 0, 16, "Select test:");

        for (int i = 0; i < DEMO_MENU_COUNT; i++) {
            int y = 32 + i * 12;
            bool sel = (i == g_selected);
            app_textf(app, 0, y, "%s %s", sel ? ">" : " ", g_menu_items[i]);
        }
        app_text(app, 0, SSD1306_HEIGHT - 8, "Up/Dn:Nav Sel:Run Esc:Back");
    } else {
        const char* test_names[] = {
            "GPIO Test",
            "I2C Scan",
            "SPI Loopback",
            "ADC Read",
        };
        app_textf(app, 0, 0, "%s", test_names[g_mode]);
        
        if (g_mode == DEMO_GPIO) {
            app_text(app, 0, 16, "Toggling GPIO 0...");
            gpio_test();
            g_mode = DEMO_MENU;
        } else if (g_mode == DEMO_I2C) {
            app_text(app, 0, 16, "Scanning I2C bus...");
            i2c_scan();
            g_mode = DEMO_MENU;
        } else if (g_mode == DEMO_SPI) {
            app_text(app, 0, 16, "SPI loopback...");
            spi_loopback();
            g_mode = DEMO_MENU;
        } else if (g_mode == DEMO_ADC) {
            app_text(app, 0, 16, "Reading ADC...");
            adc_read();
            g_mode = DEMO_MENU;
        }
    }

    app_text(app, 0, SSD1306_HEIGHT - 8, "Esc:Back");
    app_screen_end(app);
}

static void demo_cleanup(app_ctx_t* app) {
    app_display_deinit(&app->display);
    APP_INFO("Demo closed");
}

// --- App Definition ---
APP_DEFINE(demo_app, "demo", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Hardware test suite - GPIO, I2C, SPI, ADC",
           .type = APP_TYPE_TOOL, .fps = 30,
           .on_init = demo_init, .on_frame = demo_frame, .on_cleanup = demo_cleanup)