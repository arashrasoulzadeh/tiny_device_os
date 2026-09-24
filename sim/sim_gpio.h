#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sim_video.h"

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

#ifdef __cplusplus
}
#endif