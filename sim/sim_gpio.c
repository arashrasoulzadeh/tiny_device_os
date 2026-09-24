#include "sim_gpio.h"
#include "sim_video.h"
#include <stdlib.h>
#include <string.h>

#define MAX_GPIO_PINS 64

typedef struct {
    bool registered;
    bool level;
    sim_key_t key_pressed;
    sim_key_t key_released;
    bool pressed_level;
} gpio_pin_t;

static gpio_pin_t g_pins[MAX_GPIO_PINS];
static sim_gpio_callback_t g_callback = NULL;
static void* g_callback_arg = NULL;

int sim_gpio_init(void) {
    memset(g_pins, 0, sizeof(g_pins));
    return 0;
}

void sim_gpio_cleanup(void) {
}

int sim_gpio_register(int pin, bool initial_level) {
    if (pin < 0 || pin >= MAX_GPIO_PINS) return -1;
    g_pins[pin].registered = true;
    g_pins[pin].level = initial_level;
    return 0;
}

void sim_gpio_unregister(int pin) {
    if (pin >= 0 && pin < MAX_GPIO_PINS) {
        g_pins[pin].registered = false;
    }
}

bool sim_gpio_read(int pin) {
    if (pin < 0 || pin >= MAX_GPIO_PINS) return false;
    return g_pins[pin].level;
}

void sim_gpio_write(int pin, bool level) {
    if (pin < 0 || pin >= MAX_GPIO_PINS) return;
    if (g_pins[pin].level != level) {
        g_pins[pin].level = level;
        if (g_callback) {
            g_callback(pin, level, g_callback_arg);
        }
    }
}

void sim_gpio_set_key_mapping(sim_key_t key, int pin, bool pressed_level) {
    if (pin < 0 || pin >= MAX_GPIO_PINS) return;
    if (pressed_level) {
        g_pins[pin].key_pressed = key;
    } else {
        g_pins[pin].key_released = key;
    }
}

void sim_gpio_set_callback(sim_gpio_callback_t cb, void* arg) {
    g_callback = cb;
    g_callback_arg = arg;
}

void sim_gpio_handle_key(sim_key_t key, bool pressed) {
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        if (!g_pins[i].registered) continue;
        
        if (pressed && g_pins[i].key_pressed == key) {
            g_pins[i].level = true;
            if (g_callback) g_callback(i, true, g_callback_arg);
        } else if (!pressed && g_pins[i].key_released == key) {
            g_pins[i].level = false;
            if (g_callback) g_callback(i, false, g_callback_arg);
        }
    }
}