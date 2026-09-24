#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_PWM_MAX_CHANNELS 16

int sim_pwm_init(void);
void sim_pwm_cleanup(void);

int sim_pwm_set_frequency(int channel, uint32_t freq);
int sim_pwm_set_duty(int channel, uint16_t duty);
int sim_pwm_get_duty(int channel, uint16_t* duty);

void sim_pwm_enable(int channel);
void sim_pwm_disable(int channel);

#ifdef __cplusplus
}
#endif