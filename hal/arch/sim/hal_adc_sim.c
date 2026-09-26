#include "hal_adc.h"
#include <stdlib.h>
#include <string.h>

struct hal_adc {
    char path[64];
    adc_unit_t unit;
    adc_atten_t atten;
    adc_bits_width_t width;
    bool initialized;
    uint16_t simulated_value;
};

hal_adc_t* hal_adc_open(const char* path) {
    hal_adc_t* adc = calloc(1, sizeof(hal_adc_t));
    if (!adc) return NULL;
    
    strncpy(adc->path, path, sizeof(adc->path) - 1);
    adc->unit = ADC_UNIT_1;
    adc->atten = ADC_ATTEN_DB_11;
    adc->width = ADC_WIDTH_BIT_12;
    adc->initialized = false;
    adc->simulated_value = 2048; // Mid-range
    
    return adc;
}

void hal_adc_close(hal_adc_t* adc) {
    if (adc) free(adc);
}

int hal_adc_init(hal_adc_t* adc) {
    if (!adc) return -1;
    adc->initialized = true;
    return 0;
}

int hal_adc_read(hal_adc_t* adc, uint16_t* value) {
    if (!adc || !adc->initialized || !value) return -1;
    *value = adc->simulated_value;
    return 0;
}

int hal_adc_read_voltage(hal_adc_t* adc, float* voltage) {
    if (!adc || !adc->initialized || !voltage) return -1;
    // Convert 12-bit ADC value to voltage (3.3V reference)
    *voltage = (adc->simulated_value / 4095.0f) * 3.3f;
    return 0;
}

void hal_adc_set_attenuation(hal_adc_t* adc, adc_atten_t atten) {
    if (adc) adc->atten = atten;
}

void hal_adc_set_width(hal_adc_t* adc, adc_bits_width_t width) {
    if (adc) adc->width = width;
}

const char* hal_adc_get_path(const hal_adc_t* adc) {
    return adc ? adc->path : NULL;
}

int hal_adc_suspend(hal_adc_t* adc) {
    if (!adc) return -1;
    adc->initialized = false;
    return 0;
}

int hal_adc_resume(hal_adc_t* adc) {
    if (!adc) return -1;
    adc->initialized = true;
    return 0;
}