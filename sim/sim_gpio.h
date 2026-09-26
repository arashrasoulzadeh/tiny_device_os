#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sim_video.h"
#include "hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sim_gpio_callback_t)(int pin, bool level, void* arg);

int sim_gpio_init(void);
void sim_gpio_cleanup(void);

int sim_gpio_register(int pin, bool initial_level);
void sim_gpio_unregister(int pin);

bool sim_gpio_read(int pin);
void sim_gpio_write(int pin, bool level);

void sim_gpio_set_key_mapping(sim_key_t key, int pin, bool pressed_level);
void sim_gpio_set_callback(sim_gpio_callback_t cb, void* arg);

void sim_gpio_handle_key(sim_key_t key, bool pressed);

// HAL GPIO integration - uses hal_gpio_t* to match hal_gpio_callback_t
typedef void (*hal_gpio_callback_t)(int pin, void* arg);
int sim_gpio_register_hal_gpio(int pin, hal_gpio_callback_t cb, void* arg);
int sim_gpio_register_hal_gpio_with_trigger(int pin, hal_gpio_callback_t cb, void* arg, hal_gpio_irq_t trigger);
void sim_gpio_unregister_hal_gpio(int pin);

#ifdef __cplusplus
}
#endif