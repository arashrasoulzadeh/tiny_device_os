#include "sim_pwm.h"
#include <stdlib.h>

static uint32_t g_freq[SIM_PWM_MAX_CHANNELS];
static uint16_t g_duty[SIM_PWM_MAX_CHANNELS];
static bool g_enabled[SIM_PWM_MAX_CHANNELS];

int sim_pwm_init(void) {
    for (int i = 0; i < SIM_PWM_MAX_CHANNELS; i++) {
        g_freq[i] = 1000;
        g_duty[i] = 0;
        g_enabled[i] = false;
    }
    return 0;
}

void sim_pwm_cleanup(void) {
}

int sim_pwm_set_frequency(int channel, uint32_t freq) {
    if (channel < 0 || channel >= SIM_PWM_MAX_CHANNELS) return -1;
    g_freq[channel] = freq;
    return 0;
}

int sim_pwm_set_duty(int channel, uint16_t duty) {
    if (channel < 0 || channel >= SIM_PWM_MAX_CHANNELS) return -1;
    g_duty[channel] = duty;
    return 0;
}

int sim_pwm_get_duty(int channel, uint16_t* duty) {
    if (channel < 0 || channel >= SIM_PWM_MAX_CHANNELS || !duty) return -1;
    *duty = g_duty[channel];
    return 0;
}

void sim_pwm_enable(int channel) {
    if (channel >= 0 && channel < SIM_PWM_MAX_CHANNELS) {
        g_enabled[channel] = true;
    }
}

void sim_pwm_disable(int channel) {
    if (channel >= 0 && channel < SIM_PWM_MAX_CHANNELS) {
        g_enabled[channel] = false;
    }
}