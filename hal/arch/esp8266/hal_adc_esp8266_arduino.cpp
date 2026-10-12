// hal_adc_* implementation for ESP8266 Arduino framework.
// Uses Arduino analogRead for ADC.
#include "hal_adc.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

struct hal_adc {
  char path[32];
  int pin;
  adc_atten_t atten;
  adc_bits_width_t width;
  bool initialized;
};

extern "C" {

hal_adc_t *hal_adc_open(const char *path) {
  // ESP8266 only has one ADC pin (A0 = ADC0)
  hal_adc_t *adc = (hal_adc_t *)calloc(1, sizeof(hal_adc_t));
  if (!adc)
    return NULL;

  strncpy(adc->path, path, sizeof(adc->path) - 1);
  adc->pin = A0;
  adc->atten = ADC_ATTEN_DB_11;
  adc->width = ADC_WIDTH_BIT_10;
  adc->initialized = false;

  return adc;
}

void hal_adc_close(hal_adc_t *adc) {
  if (!adc)
    return;
  free(adc);
}

int hal_adc_init(hal_adc_t *adc) {
  if (!adc || adc->initialized)
    return -1;
  // ADC is always available on ESP8266 Arduino
  adc->initialized = true;
  return 0;
}

int hal_adc_read(hal_adc_t *adc, uint16_t *value) {
  if (!adc || !adc->initialized || !value)
    return -1;
  // ESP8266 ADC is 10-bit (0-1023), 1V max input
  *value = (uint16_t)analogRead(A0);
  return 0;
}

int hal_adc_read_voltage(hal_adc_t *adc, float *voltage) {
  if (!adc || !adc->initialized || !voltage)
    return -1;
  int raw = analogRead(A0);
  // ESP8266 ADC: 10-bit, 1V reference = 1023 max
  // Convert to volts
  *voltage = (float)raw / 1023.0f;
  return 0;
}

void hal_adc_set_attenuation(hal_adc_t *adc, adc_atten_t atten) {
  if (!adc)
    return;
  adc->atten = atten;
  // ESP8266 Arduino doesn't support attenuation configuration
}

void hal_adc_set_width(hal_adc_t *adc, adc_bits_width_t width) {
  if (!adc)
    return;
  adc->width = width;
  // ESP8266 is fixed 10-bit
}

const char *hal_adc_get_path(const hal_adc_t *adc) {
  return adc ? adc->path : NULL;
}

int hal_adc_suspend(hal_adc_t *adc) {
  if (!adc)
    return -1;
  adc->initialized = false;
  return 0;
}

int hal_adc_resume(hal_adc_t *adc) {
  if (!adc)
    return -1;
  adc->initialized = true;
  return 0;
}

} // extern "C"
