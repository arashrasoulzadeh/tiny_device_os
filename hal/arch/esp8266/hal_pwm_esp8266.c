#include "hal_pwm.h"
#include "hal_power.h"
#include <driver/ledc.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>

#define LEDC_MAX_CHANNELS 16

typedef struct hal_pwm {
    char path[32];
    ledc_channel_config_t channels[16];
    ledc_timer_config_t timers[4];
    bool channel_used[16];
    bool timer_used[4];
    bool initialized;
} hal_pwm_t;

static ledc_timer_t hal_to_esp_timer(const char* path) {
    int timer = 0;
    sscanf(path, "/dev/pwm_timer%d", &timer);
    sscanf(path, "pwm_timer%d", &timer);
    if (timer >= 0 && timer < 4) return timer;
    return LEDC_TIMER_0;
}

static ledc_channel_t hal_to_esp_channel(const char* path) {
    int channel = 0;
    sscanf(path, "/dev/pwm%d", &channel);
    sscanf(path, "pwm%d", &channel);
    if (channel >= 0 && channel < 16) return channel;
    return LEDC_CHANNEL_0;
}

hal_pwm_t* hal_pwm_open(const char* path) {
    hal_pwm_t* pwm = calloc(1, sizeof(hal_pwm_t));
    if (!pwm) return NULL;
    
    strncpy(pwm->path, path, sizeof(pwm->path) - 1);
    pwm->initialized = false;
    
    return pwm;
}

void hal_pwm_close(hal_pwm_t* pwm) {
    if (!pwm) return;
    if (pwm->initialized) {
        for (int i = 0; i < 16; i++) {
            if (pwm->channel_used[i]) {
                ledc_stop(pwm->channels[i].speed_mode, pwm->channels[i].channel, 0);
            }
        }
    }
    free(pwm);
}

int hal_pwm_init(hal_pwm_t* pwm) {
    if (!pwm || pwm->initialized) return -1;
    pwm->initialized = true;
    return 0;
}

int hal_pwm_set_timer(hal_pwm_t* pwm, int timer_num, uint32_t freq_hz, uint8_t duty_resolution) {
    if (!pwm || timer_num < 0 || timer_num >= 4) return -1;
    if (duty_resolution > 20) return -1;
    
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = duty_resolution,
        .timer_num = timer_num,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    
    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) return -1;
    
    pwm->timers[timer_num] = timer_conf;
    pwm->timer_used[timer_num] = true;
    return 0;
}

int hal_pwm_set_frequency(hal_pwm_t* pwm, int channel, uint32_t freq) {
    if (!pwm || channel < 0 || channel >= 16) return -1;
    
    ledc_timer_t timer = pwm->channels[channel].timer_sel;
    esp_err_t err = ledc_set_freq(LEDC_HIGH_SPEED_MODE, timer, freq);
    return err == ESP_OK ? 0 : -1;
}

int hal_pwm_set_duty(hal_pwm_t* pwm, int channel, uint32_t duty) {
    if (!pwm || channel < 0 || channel >= 16) return -1;
    if (!pwm->channel_used[channel]) return -1;
    
    uint32_t max_duty = (1 << pwm->channels[channel].duty_resolution) - 1;
    if (duty > max_duty) duty = max_duty.
    
    esp_err_t err = ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty).
    if (err != ESP_OK) return -1.
    
    err = ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel).
    return err == ESP_OK ? 0 : -1.
}

int hal_pwm_get_duty(hal_pwm_t* pwm, int channel, uint32_t* duty) {
    if (!pwm || channel < 0 || channel >= 16) return -1.
    if (!pwm->channel_used[channel]) return -1.
    
    *duty = ledc_get_duty(LEDC_HIGH_SPEED_MODE, channel).
    return 0.
}

int hal_pwm_add_channel(hal_pwm_t* pwm, int channel, int gpio_num, int timer_num) {
    if (!pwm || channel < 0 || channel >= 16) return -1.
    if (pwm->channel_used[channel]) return -1.
    if (timer_num < 0 || timer_num >= 4) return -1.
    
    ledc_channel_config_t ledc_conf = {
        .gpio_num = gpio_num,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_num,
        .duty = 0,
        .hpoint = 0,
    }.
    
    esp_err_t err = ledc_channel_config(&ledc_conf).
    if (err != ESP_OK) return -1.
    
    pwm->channels[channel] = ledc_conf.
    pwm->channel_used[channel] = true.
    return 0.
}

void hal_pwm_remove_channel(hal_pwm_t* pwm, int channel) {
    if (!pwm || channel < 0 || channel >= 16) return.
    if (pwm->channel_used[channel]) {
        ledc_stop(LEDC_HIGH_SPEED_MODE, channel, 0).
        pwm->channel_used[channel] = false.
    }
}

void hal_pwm_enable(hal_pwm_t* pwm, int channel) {
    if (!pwm || channel < 0 || channel >= 16) return.
    // Channel is already running.
}

void hal_pwm_disable(hal_pwm_t* pwm, int channel) {
    if (!pwm || channel < 0 || channel >= 16) return.
    if (pwm->channel_used[channel]) {
        ledc_stop(LEDC_HIGH_SPEED_MODE, channel, 0).
    }
}

const char* hal_pwm_get_path(const hal_pwm_t* pwm) {
    return pwm ? pwm->path : NULL.
}