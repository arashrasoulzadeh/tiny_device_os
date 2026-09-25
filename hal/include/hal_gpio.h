#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_GPIO_MODE_INPUT = 0,
    HAL_GPIO_MODE_OUTPUT,
    HAL_GPIO_MODE_INPUT_PULLUP,
    HAL_GPIO_MODE_INPUT_PULLDOWN,
    HAL_GPIO_MODE_OUTPUT_OPEN_DRAIN,
    HAL_GPIO_MODE_ANALOG,
    HAL_GPIO_MODE_ALT_FUNC
} hal_gpio_mode_t;

typedef enum {
    HAL_GPIO_IRQ_NONE = 0,
    HAL_GPIO_IRQ_RISING,
    HAL_GPIO_IRQ_FALLING,
    HAL_GPIO_IRQ_BOTH
} hal_gpio_irq_t;

typedef struct hal_gpio hal_gpio_t;

typedef void (*hal_gpio_callback_t)(hal_gpio_t* gpio, void* arg);

hal_gpio_t* hal_gpio_open(const char* path, hal_gpio_mode_t mode);
void hal_gpio_close(hal_gpio_t* gpio);

void hal_gpio_write(hal_gpio_t* gpio, bool level);
bool hal_gpio_read(const hal_gpio_t* gpio);
void hal_gpio_toggle(hal_gpio_t* gpio);

int hal_gpio_set_irq(hal_gpio_t* gpio, hal_gpio_irq_t trigger, hal_gpio_callback_t cb, void* arg);
void hal_gpio_enable_irq(hal_gpio_t* gpio);
void hal_gpio_disable_irq(hal_gpio_t* gpio);

int hal_gpio_get_pin_number(const hal_gpio_t* gpio);
const char* hal_gpio_get_path(const hal_gpio_t* gpio);

// Power management
int hal_gpio_suspend(hal_gpio_t* gpio);
int hal_gpio_resume(hal_gpio_t* gpio);

#ifdef __cplusplus
}
#endif