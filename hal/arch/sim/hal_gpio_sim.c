#include "hal_gpio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct hal_gpio {
    char path[64];
    hal_gpio_mode_t mode;
    bool level;
    hal_gpio_irq_t irq_trigger;
    hal_gpio_callback_t irq_cb;
    void* irq_arg;
    int pin_number;
    bool irq_enabled;
};

static int g_next_pin = 0;

int extract_pin_from_path(const char* path) {
    // Extract pin number from path like "/dev/gpio1" -> 1
    if (!path) return -1;
    const char* pin_str = strrchr(path, 'o'); // Find last 'o' in "gpio"
    if (pin_str) {
        pin_str++; // Move past 'o'
        return atoi(pin_str);
    }
    return -1;
}

hal_gpio_t* hal_gpio_open(const char* path, hal_gpio_mode_t mode) {
    hal_gpio_t* gpio = calloc(1, sizeof(hal_gpio_t));
    if (!gpio) return NULL;
    
    strncpy(gpio->path, path, sizeof(gpio->path) - 1);
    gpio->mode = mode;
    gpio->level = false;
    gpio->pin_number = extract_pin_from_path(path);
    if (gpio->pin_number < 0) {
        gpio->pin_number = g_next_pin++;
    }
    gpio->irq_enabled = false;
    
    return gpio;
}

void hal_gpio_close(hal_gpio_t* gpio) {
    if (gpio) {
        free(gpio);
    }
}

void hal_gpio_write(hal_gpio_t* gpio, bool level) {
    if (!gpio) return;
    gpio->level = level;
}

bool hal_gpio_read(const hal_gpio_t* gpio) {
    return gpio ? gpio->level : false;
}

void hal_gpio_toggle(hal_gpio_t* gpio) {
    if (gpio) {
        gpio->level = !gpio->level;
    }
}

int hal_gpio_set_irq(hal_gpio_t* gpio, hal_gpio_irq_t trigger, hal_gpio_callback_t cb, void* arg) {
    if (!gpio) return -1;
    gpio->irq_trigger = trigger;
    gpio->irq_cb = cb;
    gpio->irq_arg = arg;
    return 0;
}

void hal_gpio_enable_irq(hal_gpio_t* gpio) {
    if (gpio) gpio->irq_enabled = true;
}

void hal_gpio_disable_irq(hal_gpio_t* gpio) {
    if (gpio) gpio->irq_enabled = false;
}

int hal_gpio_get_pin_number(const hal_gpio_t* gpio) {
    return gpio ? gpio->pin_number : -1;
}

const char* hal_gpio_get_path(const hal_gpio_t* gpio) {
    return gpio ? gpio->path : NULL;
}

int hal_gpio_suspend(hal_gpio_t* gpio) {
    if (!gpio) return -1;
    // In simulator, nothing to do
    return 0;
}

int hal_gpio_resume(hal_gpio_t* gpio) {
    if (!gpio) return -1;
    // In simulator, nothing to do
    return 0;
}