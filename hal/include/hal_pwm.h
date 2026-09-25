#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LEDC_TIMER_0 = 0,
    LEDC_TIMER_1,
    LEDC_TIMER_2,
    LEDC_TIMER_3
} ledc_timer_t;

typedef enum {
    LEDC_CHANNEL_0 = 0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3,
    LEDC_CHANNEL_4,
    LEDC_CHANNEL_5,
    LEDC_CHANNEL_6,
    LEDC_CHANNEL_7,
    LEDC_CHANNEL_8,
    LEDC_CHANNEL_9,
    LEDC_CHANNEL_10,
    LEDC_CHANNEL_11,
    LEDC_CHANNEL_12,
    LEDC_CHANNEL_13,
    LEDC_CHANNEL_14,
    LEDC_CHANNEL_15
} ledc_channel_t;

typedef struct hal_pwm hal_pwm_t;

hal_pwm_t* hal_pwm_open(const char* path);
void hal_pwm_close(hal_pwm_t* pwm);

int hal_pwm_init(hal_pwm_t* pwm);

int hal_pwm_set_timer(hal_pwm_t* pwm, int timer_num, uint32_t freq_hz, uint8_t duty_resolution);
int hal_pwm_set_frequency(hal_pwm_t* pwm, int channel, uint32_t freq);
int hal_pwm_set_duty(hal_pwm_t* pwm, int channel, uint32_t duty);
int hal_pwm_get_duty(hal_pwm_t* pwm, int channel, uint32_t* duty);

int hal_pwm_add_channel(hal_pwm_t* pwm, int channel, int gpio_num, int timer_num);
void hal_pwm_remove_channel(hal_pwm_t* pwm, int channel);

void hal_pwm_enable(hal_pwm_t* pwm, int channel);
void hal_pwm_disable(hal_pwm_t* pwm, int channel);

const char* hal_pwm_get_path(const hal_pwm_t* pwm);

#ifdef __cplusplus
}
#endif