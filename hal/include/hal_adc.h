#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADC_UNIT_1 = 0,
    ADC_UNIT_2
} adc_unit_t;

typedef enum {
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3,
    ADC_CHANNEL_4,
    ADC_CHANNEL_5,
    ADC_CHANNEL_6,
    ADC_CHANNEL_7,
    ADC_CHANNEL_MAX
} adc_channel_t;

typedef enum {
    ADC_ATTEN_DB_0 = 0,
    ADC_ATTEN_DB_2_5,
    ADC_ATTEN_DB_6,
    ADC_ATTEN_DB_11
} adc_atten_t;

typedef enum {
    ADC_WIDTH_BIT_9 = 0,
    ADC_WIDTH_BIT_10,
    ADC_WIDTH_BIT_11,
    ADC_WIDTH_BIT_12
} adc_bits_width_t;

typedef struct hal_adc hal_adc_t;

hal_adc_t* hal_adc_open(const char* path);
void hal_adc_close(hal_adc_t* adc);

int hal_adc_init(hal_adc_t* adc);
int hal_adc_read(hal_adc_t* adc, uint16_t* value);
int hal_adc_read_voltage(hal_adc_t* adc, float* voltage);

void hal_adc_set_attenuation(hal_adc_t* adc, adc_atten_t atten);
void hal_adc_set_width(hal_adc_t* adc, adc_bits_width_t width);

const char* hal_adc_get_path(const hal_adc_t* adc);

#ifdef __cplusplus
}
#endif