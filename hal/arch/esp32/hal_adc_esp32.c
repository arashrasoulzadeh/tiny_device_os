#include "hal_adc.h"
#include "hal_power.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>

typedef struct hal_adc {
    char path[32];
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t atten;
    adc_bits_width_t width;
    esp_adc_cal_characteristics_t* chars;
    bool initialized;
} hal_adc_t;

static adc_unit_t hal_to_esp_unit(const char* path) {
    if (strstr(path, "adc2")) return ADC_UNIT_2;
    return ADC_UNIT_1;
}

static adc_channel_t hal_to_esp_channel(const char* path) {
    int channel = 0;
    sscanf(path, "/dev/adc%d", &channel);
    sscanf(path, "adc%d", &channel);
    if (channel >= 0 && channel < ADC_CHANNEL_MAX) return channel;
    return ADC_CHANNEL_0;
}

hal_adc_t* hal_adc_open(const char* path) {
    hal_adc_t* adc = calloc(1, sizeof(hal_adc_t));
    if (!adc) return NULL;
    
    strncpy(adc->path, path, sizeof(adc->path) - 1);
    adc->unit = hal_to_esp_unit(path);
    adc->channel = hal_to_esp_channel(path);
    adc->atten = ADC_ATTEN_DB_11;
    adc->width = ADC_WIDTH_BIT_12;
    adc->chars = NULL;
    adc->initialized = false;
    
    return adc;
}

void hal_adc_close(hal_adc_t* adc) {
    if (!adc) return;
    if (adc->chars) free(adc->chars);
    if (adc->initialized) {
        adc1_config_channel_atten(adc->channel, ADC_ATTEN_DB_0);
    }
    free(adc);
}

int hal_adc_init(hal_adc_t* adc) {
    if (!adc || adc->initialized) return -1;
    
    if (adc->unit == ADC_UNIT_1) {
        esp_err_t err = adc1_config_width(adc->width);
        if (err != ESP_OK) return -1;
        err = adc1_config_channel_atten(adc->channel, adc->atten);
        if (err != ESP_OK) return -1;
    } else {
        esp_err_t err = adc2_config_channel_atten(adc->channel, adc->atten);
        if (err != ESP_OK) return -1;
    }
    
    adc->chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(adc->unit, adc->atten, adc->width, 1100, adc->chars);
    
    adc->initialized = true;
    return 0;
}

int hal_adc_read(hal_adc_t* adc, uint16_t* value) {
    if (!adc || !value || !adc->initialized) return -1;
    
    int raw = 0;
    if (adc->unit == ADC_UNIT_1) {
        raw = adc1_get_raw(adc->channel);
    } else {
        int raw2;
        esp_err_t err = adc2_get_raw(adc->channel, ADC_WIDTH_BIT_12, &raw2);
        if (err != ESP_OK) return -1;
        raw = raw2;
    }
    
    *value = (uint16_t)raw;
    return 0;
}

int hal_adc_read_voltage(hal_adc_t* adc, float* voltage) {
    if (!adc || !voltage || !adc->initialized) return -1;
    
    int raw = 0;
    if (adc->unit == ADC_UNIT_1) {
        raw = adc1_get_raw(adc->channel);
    } else {
        int raw2;
        esp_err_t err = adc2_get_raw(adc->channel, ADC_WIDTH_BIT_12, &raw2);
        if (err != ESP_OK) return -1;
        raw = raw2;
    }
    
    if (adc->chars) {
        uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, adc->chars);
        *voltage = voltage_mv / 1000.0f;
    } else {
        *voltage = (raw / 4095.0f) * 3.3f;
    }
    
    return 0;
}

void hal_adc_set_attenuation(hal_adc_t* adc, adc_atten_t atten) {
    if (!adc) return;
    adc->atten = atten;
    if (adc->initialized) {
        if (adc->unit == ADC_UNIT_1) {
            adc1_config_channel_atten(adc->channel, atten);
        } else {
            adc2_config_channel_atten(adc->channel, atten);
        }
        if (adc->chars) {
            esp_adc_cal_characterize(adc->unit, atten, adc->width, 1100, adc->chars);
        }
    }
}

void hal_adc_set_width(hal_adc_t* adc, adc_bits_width_t width) {
    if (!adc) return;
    adc->width = width;
    if (adc->initialized && adc->unit == ADC_UNIT_1) {
        adc1_config_width(width);
        if (adc->chars) {
            esp_adc_cal_characterize(adc->unit, adc->atten, width, 1100, adc->chars);
        }
    }
}

const char* hal_adc_get_path(const hal_adc_t* adc) {
    return adc ? adc->path : NULL;
}