#include "hal_gpio.h"
#include "hal_power.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdlib.h>

#define GPIO_PIN_TO_PORT(pin) ((pin) >> 3)
#define GPIO_PIN_TO_BIT(pin) ((pin) & 0x07)

typedef struct hal_gpio {
    char path[32];
    hal_gpio_mode_t mode;
    uint8_t pin;
    volatile uint8_t* port_reg;
    volatile uint8_t* ddr_reg;
    volatile uint8_t* pin_reg;
    uint8_t bit_mask;
    hal_gpio_irq_t irq_trigger;
    hal_gpio_callback_t irq_cb;
    void* irq_arg;
    bool irq_enabled;
} hal_gpio_t;

static void hal_gpio_configure_registers(hal_gpio_t* gpio) {
    gpio->port_reg = portOutputRegister(GPIO_PIN_TO_PORT(gpio->pin));
    gpio->ddr_reg = portModeRegister(GPIO_PIN_TO_PORT(gpio->pin));
    gpio->pin_reg = portInputRegister(GPIO_PIN_TO_PORT(gpio->pin));
    gpio->bit_mask = (1 << (gpio->pin & 0x07));
}

hal_gpio_t* hal_gpio_open(const char* path, hal_gpio_mode_t mode) {
    int pin = -1;
    if (sscanf(path, "/dev/gpio%d", &pin) != 1 && sscanf(path, "gpio%d", &pin) != 1) {
        return NULL;
    }
    if (pin < 0 || pin >= 86) {
        return NULL;
    }

    hal_gpio_t* gpio = calloc(1, sizeof(hal_gpio_t));
    if (!gpio) return NULL;

    strncpy(gpio->path, path, sizeof(gpio->path) - 1);
    gpio->mode = mode;
    gpio->pin = pin;
    gpio->irq_trigger = HAL_GPIO_IRQ_NONE;
    gpio->irq_cb = NULL;
    gpio->irq_arg = NULL;
    gpio->irq_enabled = false;

    hal_gpio_configure_registers(gpio);

    if (mode == HAL_GPIO_MODE_OUTPUT || mode == HAL_GPIO_MODE_OUTPUT_OPEN_DRAIN) {
        *gpio->ddr_reg |= gpio->bit_mask;
    } else {
        *gpio->ddr_reg &= ~gpio->bit_mask;
        if (mode == HAL_GPIO_MODE_INPUT_PULLUP) {
            // Enable pull-up
            volatile uint8_t* port_reg = portOutputRegister(GPIO_PIN_TO_PORT(pin));
            *port_reg |= gpio->bit_mask;
        } else if (mode == HAL_GPIO_MODE_INPUT_PULLDOWN) {
            // AVR doesn't have built-in pull-down, external resistor needed
        }
    }

    return gpio;
}

void hal_gpio_close(hal_gpio_t* gpio) {
    if (!gpio) return;
    // Disable interrupts
    if (gpio->irq_enabled) {
        // Disable pin change interrupt
    }
    free(gpio);
}

void hal_gpio_write(hal_gpio_t* gpio, bool level) {
    if (!gpio) return;
    if (level) {
        *gpio->port_reg |= gpio->bit_mask;
    } else {
        *gpio->port_reg &= ~gpio->bit_mask;
    }
}

bool hal_gpio_read(const hal_gpio_t* gpio) {
    if (!gpio) return false;
    return (*gpio->pin_reg & gpio->bit_mask) != 0;
}

void hal_gpio_toggle(hal_gpio_t* gpio) {
    if (!gpio) return;
    *gpio->port_reg ^= gpio->bit_mask;
}

int hal_gpio_set_irq(hal_gpio_t* gpio, hal_gpio_irq_t trigger, hal_gpio_callback_t cb, void* arg) {
    if (!gpio || trigger == HAL_GPIO_IRQ_NONE) return -1;
    
    gpio->irq_trigger = trigger;
    gpio->irq_cb = cb;
    gpio->irq_arg = arg;
    gpio->irq_enabled = true;
    
    // Configure pin change interrupt
    // This is simplified - actual implementation would use PCINT
    return 0;
}

void hal_gpio_enable_irq(hal_gpio_t* gpio) {
    if (!gpio) return;
    gpio->irq_enabled = true;
}

void hal_gpio_disable_irq(hal_gpio_t* gpio) {
    if (!gpio) return;
    gpio->irq_enabled = false;
}

int hal_gpio_get_pin_number(const hal_gpio_t* gpio) {
    return gpio ? gpio->pin : -1;
}

const char* hal_gpio_get_path(const hal_gpio_t* gpio) {
    return gpio ? gpio->path : NULL;
}

int hal_gpio_suspend(hal_gpio_t* gpio) {
    if (!gpio) return -1;
    return 0;
}

int hal_gpio_resume(hal_gpio_t* gpio) {
    if (!gpio) return -1;
    return 0;
}