#include "sim_adc.h"
#include <stdlib.h>

static uint16_t g_values[SIM_ADC_MAX_CHANNELS];

int sim_adc_init(void) {
    for (int i = 0; i < SIM_ADC_MAX_CHANNELS; i++) {
        g_values[i] = 0;
    }
    return 0;
}

void sim_adc_cleanup(void) {
}

int sim_adc_read(int channel, uint16_t* value) {
    if (channel < 0 || channel >= SIM_ADC_MAX_CHANNELS || !value) return -1;
    *value = g_values[channel];
    return 0;
}

int sim_adc_read_voltage(int channel, float* voltage) {
    if (channel < 0 || channel >= SIM_ADC_MAX_CHANNELS || !voltage) return -1;
    *voltage = (g_values[channel] / 4095.0f) * 3.3f;
    return 0;
}

void sim_adc_set_value(int channel, uint16_t value) {
    if (channel >= 0 && channel < SIM_ADC_MAX_CHANNELS) {
        g_values[channel] = value;
    }
}

void sim_adc_set_voltage(int channel, float voltage) {
    if (channel >= 0 && channel < SIM_ADC_MAX_CHANNELS) {
        g_values[channel] = (uint16_t)((voltage / 3.3f) * 4095.0f);
    }
}