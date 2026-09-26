#include "hal_pwm.h"
#include <stdlib.h>
#include <string.h>

#define PWM_MAX_CHANNELS 16
#define PWM_MAX_TIMERS 4

struct hal_pwm {
    char path[64];
    bool initialized;
    uint32_t timer_freq[PWM_MAX_TIMERS];
    uint8_t timer_duty_res[PWM_MAX_TIMERS];
    uint32_t channel_duty[PWM_MAX_CHANNELS];
    uint32_t channel_freq[PWM_MAX_CHANNELS];
    int channel_gpio[PWM_MAX_CHANNELS];
    int channel_timer[PWM_MAX_CHANNELS];
    bool channel_enabled[PWM_MAX_CHANNELS];
};

hal_pwm_t* hal_pwm_open(const char* path) {
    hal_pwm_t* pwm = calloc(1, sizeof(hal_pwm_t));
    if (!pwm) return NULL;
    
    strncpy(pwm->path, path, sizeof(pwm->path) - 1);
    pwm->initialized = false;
    
    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        pwm->channel_gpio[i] = -1;
        pwm->channel_timer[i] = -1;
    }
    
    return pwm;
}

void hal_pwm_close(hal_pwm_t* pwm) {
    if (pwm) free(pwm);
}

int hal_pwm_init(hal_pwm_t* pwm) {
    if (!pwm) return -1;
    pwm->initialized = true;
    return 0;
}

int hal_pwm_set_timer(hal_pwm_t* pwm, int timer_num, uint32_t freq_hz, uint8_t duty_resolution) {
    if (!pwm || timer_num < 0 || timer_num >= PWM_MAX_TIMERS) return -1;
    pwm->timer_freq[timer_num] = freq_hz;
    pwm->timer_duty_res[timer_num] = duty_resolution;
    return 0;
}

int hal_pwm_set_frequency(hal_pwm_t* pwm, int channel, uint32_t freq) {
    if (!pwm || channel < 0 || channel >= PWM_MAX_CHANNELS) return -1;
    pwm->channel_freq[channel] = freq;
    return 0;
}

int hal_pwm_set_duty(hal_pwm_t* pwm, int channel, uint32_t duty) {
    if (!pwm || channel < 0 || channel >= PWM_MAX_CHANNELS) return -1;
    pwm->channel_duty[channel] = duty;
    return 0;
}

int hal_pwm_get_duty(hal_pwm_t* pwm, int channel, uint32_t* duty) {
    if (!pwm || channel < 0 || channel >= PWM_MAX_CHANNELS || !duty) return -1;
    *duty = pwm->channel_duty[channel];
    return 0;
}

int hal_pwm_add_channel(hal_pwm_t* pwm, int channel, int gpio_num, int timer_num) {
    if (!pwm || channel < 0 || channel >= PWM_MAX_CHANNELS) return -1;
    pwm->channel_gpio[channel] = gpio_num;
    pwm->channel_timer[channel] = timer_num;
    return 0;
}

void hal_pwm_remove_channel(hal_pwm_t* pwm, int channel) {
    if (!pwm || channel < 0 || channel >= PWM_MAX_CHANNELS) return;
    pwm->channel_gpio[channel] = -1;
    pwm->channel_timer[channel] = -1;
    pwm->channel_enabled[channel] = false;
}

void hal_pwm_enable(hal_pwm_t* pwm, int channel) {
    if (!pwm || channel < 0 || channel >= PWM_MAX_CHANNELS) return;
    pwm->channel_enabled[channel] = true;
}

void hal_pwm_disable(hal_pwm_t* pwm, int channel) {
    if (!pwm || channel < 0 || channel >= PWM_MAX_CHANNELS) return;
    pwm->channel_enabled[channel] = false;
}

const char* hal_pwm_get_path(const hal_pwm_t* pwm) {
    return pwm ? pwm->path : NULL;
}

int hal_pwm_suspend(hal_pwm_t* pwm) {
    if (!pwm) return -1;
    pwm->initialized = false;
    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        pwm->channel_enabled[i] = false;
    }
    return 0;
}

int hal_pwm_resume(hal_pwm_t* pwm) {
    if (!pwm) return -1;
    pwm->initialized = true;
    return 0;
}