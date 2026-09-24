#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_ADC_MAX_CHANNELS 8

int sim_adc_init(void);
void sim_adc_cleanup(void);

int sim_adc_read(int channel, uint16_t* value);
int sim_adc_read_voltage(int channel, float* voltage);

void sim_adc_set_value(int channel, uint16_t value);
void sim_adc_set_voltage(int channel, float voltage);

#ifdef __cplusplus
}
#endif