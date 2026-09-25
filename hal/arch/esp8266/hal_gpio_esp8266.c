#include "hal_gpio.h"
#include "hal_power.h"
#include <driver/gpio.h>
#include <esp_intr_alloc.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdlib.h>

typedef struct hal_gpio {
    char path[32];
    hal_gpio_mode_t mode;
    int pin;
    hal_gpio_irq_t irq_trigger;
    hal_gpio_callback_t irq_cb;
    void* irq_arg;
    intr_handle_t irq_handle;
    bool irq_enabled;
    bool wake_enabled;
} hal_gpio_t;

static gpio_mode_t hal_to_esp_mode(hal_gpio_mode_t mode) {
    switch (mode) {
        case HAL_GPIO_MODE_INPUT: return GPIO_MODE_INPUT;
        case HAL_GPIO_MODE_OUTPUT: return GPIO_MODE_OUTPUT;
        case HAL_GPIO_MODE_INPUT_PULLUP: return GPIO_MODE_INPUT | GPIO_PULLUP_ONLY;
        case HAL_GPIO_MODE_INPUT_PULLDOWN: return GPIO_MODE_INPUT | GPIO_PULLDOWN_ONLY;
        case HAL_GPIO_MODE_OUTPUT_OPEN_DRAIN: return GPIO_MODE_OUTPUT_OD;
        case HAL_GPIO_MODE_ANALOG: return GPIO_MODE_DISABLE;
        case HAL_GPIO_MODE_ALT_FUNC: return GPIO_MODE_INPUT;
        default: return GPIO_MODE_DISABLE;
    }
}

static gpio_int_type_t hal_to_esp_irq(hal_gpio_irq_t trigger) {
    switch (trigger) {
        case HAL_GPIO_IRQ_RISING: return GPIO_INTR_POSEDGE;
        case HAL_GPIO_IRQ_FALLING: return GPIO_INTR_NEGEDGE;
        case HAL_GPIO_IRQ_BOTH: return GPIO_INTR_ANYEDGE;
        default: return GPIO_INTR_DISABLE;
    }
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    hal_gpio_t* gpio = (hal_gpio_t*)arg;
    if (gpio->irq_cb && gpio->irq_enabled) {
        bool level = gpio_get_level(gpio->pin);
        gpio->irq_cb(gpio, gpio->irq_arg);
    }
}

hal_gpio_t* hal_gpio_open(const char* path, hal_gpio_mode_t mode) {
    int pin = -1;
    if (sscanf(path, "/dev/gpio%d", &pin) != 1 && sscanf(path, "gpio%d", &pin) != 1) {
        return NULL;
    }
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return NULL;
    }

    hal_gpio_t* gpio = calloc(1, sizeof(hal_gpio_t));
    if (!gpio) return NULL;

    strncpy(gpio->path, path, sizeof(gpio->path) - 1);
    gpio->mode = mode;
    gpio->pin = pin;
    gpio->irq_handle = NULL;
    gpio->irq_enabled = false;
    gpio->wake_enabled = false;

    gpio_config_t config = {
        .pin_bit_mask = (1ULL << pin),
        .mode = hal_to_esp_mode(mode),
        .pull_up_en = (mode == HAL_GPIO_MODE_INPUT_PULLUP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (mode == HAL_GPIO_MODE_INPUT_PULLDOWN) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        free(gpio);
        return NULL;
    }

    return gpio;
}

void hal_gpio_close(hal_gpio_t* gpio) {
    if (!gpio) return;
    if (gpio->irq_handle) {
        gpio_isr_handler_remove(gpio->pin);
        esp_intr_free(gpio->irq_handle);
    }
    if (gpio->wake_enabled) {
        gpio_wakeup_disable(gpio->pin);
    }
    gpio_reset_pin(gpio->pin);
    free(gpio);
}

void hal_gpio_write(hal_gpio_t* gpio, bool level) {
    if (!gpio) return;
    gpio_set_level(gpio->pin, level);
}

bool hal_gpio_read(const hal_gpio_t* gpio) {
    if (!gpio) return false;
    return gpio_get_level(gpio->pin);
}

void hal_gpio_toggle(hal_gpio_t* gpio) {
    if (!gpio) return;
    gpio_set_level(gpio->pin, !gpio_get_level(gpio->pin));
}

int hal_gpio_set_irq(hal_gpio_t* gpio, hal_gpio_irq_t trigger, hal_gpio_callback_t cb, void* arg) {
    if (!gpio || trigger == HAL_GPIO_IRQ_NONE) return -1;
    
    gpio->irq_trigger = trigger;
    gpio->irq_cb = cb;
    gpio->irq_arg = arg;
    
    gpio_int_type_t esp_trigger = GPIO_INTR_DISABLE;
    switch (trigger) {
        case HAL_GPIO_IRQ_RISING: gpio->irq_trigger = GPIO_INTR_POSEDGE; break;
        case HAL_GPIO_IRQ_FALLING: gpio->irq_trigger = GPIO_INTR_NEGEDGE; break;
        case HAL_GPIO_IRQ_BOTH: gpio->irq_trigger = GPIO_INTR_ANYEDGE; break;
        default: gpio->irq_trigger = GPIO_INTR_DISABLE;
    }
    
    if (gpio->irq_handle) {
        gpio_isr_handler_remove(gpio->pin);
        esp_intr_free(gpio->irq_handle);
        gpio->irq_handle = NULL;
    }
    
    if (gpio->irq_trigger != GPIO_INTR_DISABLE) {
        esp_err_t err = gpio_isr_handler_add(gpio->pin, gpio_isr_handler, gpio);
        if (err != ESP_OK) return -1;
        err = gpio_set_intr_type(gpio->pin, gpio->irq_trigger);
        if (err != ESP_OK) return -1;
    }
    
    return 0;
}

void hal_gpio_enable_irq(hal_gpio_t* gpio) {
    if (!gpio) return;
    gpio->irq_enabled = true;
    if (gpio->irq_trigger != HAL_GPIO_IRQ_NONE) {
        gpio_intr_enable(gpio->pin);
    }
}

void hal_gpio_disable_irq(hal_gpio_t* gpio) {
    if (!gpio) return;
    gpio->irq_enabled = false;
    gpio_intr_disable(gpio->pin);
}

int hal_gpio_get_pin_number(const hal_gpio_t* gpio) {
    return gpio ? gpio->pin : -1;
}

const char* hal_gpio_get_path(const hal_gpio_t* gpio) {
    return gpio ? gpio->path : NULL;
}

int hal_gpio_suspend(hal_gpio_t* gpio) {
    if (!gpio) return -1;
    if (gpio->irq_handle) {
        gpio_isr_handler_remove(gpio->pin);
        esp_intr_free(gpio->irq_handle);
        gpio->irq_handle = NULL;
    }
    return 0;
}

int hal_gpio_resume(hal_gpio_t* gpio) {
    if (!gpio || gpio->irq_trigger == HAL_GPIO_IRQ_NONE) return -1;
    esp_err_t err = gpio_isr_handler_add(gpio->pin, gpio_isr_handler, gpio);
    if (err != ESP_OK) return -1;
    err = gpio_set_intr_type(gpio->pin, gpio->irq_trigger);
    return err == ESP_OK ? 0 : -1;
}

int hal_gpio_set_wake(hal_gpio_t* gpio, hal_gpio_irq_t trigger) {
    if (!gpio) return -1;
    
    gpio_pull_mode_t pull = GPIO_PULLUP_ONLY;
    if (trigger == HAL_GPIO_IRQ_FALLING) {
        pull = GPIO_PULLDOWN_ONLY;
    }
    
    gpio_pullup_en(gpio->pin);
    gpio_pulldown_en(gpio->pin);
    
    gpio_wakeup_enable(gpio->pin, (trigger == HAL_GPIO_IRQ_RISING) ? GPIO_INTR_HIGH_LEVEL : 
                        (trigger == HAL_GPIO_IRQ_FALLING) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_ANYEDGE);
    gpio->wake_enabled = true;
    
    return 0;
}

void hal_gpio_clear_wake(hal_gpio_t* gpio) {
    if (!gpio) return;
    gpio_wakeup_disable(gpio->pin);
    gpio->wake_enabled = false;
}