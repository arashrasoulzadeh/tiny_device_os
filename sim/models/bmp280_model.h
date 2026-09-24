#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMP280_I2C_ADDR 0x76
#define BMP280_CHIP_ID 0x58

typedef enum {
    BMP280_REG_CALIB_00 = 0x88,
    BMP280_REG_CALIB_26 = 0xA1,
    BMP280_REG_ID = 0xD0,
    BMP280_REG_RESET = 0xE0,
    BMP280_REG_STATUS = 0xF3,
    BMP280_REG_CTRL_MEAS = 0xF4,
    BMP280_REG_CONFIG = 0xF5,
    BMP280_REG_PRESS_MSB = 0xF7,
    BMP280_REG_PRESS_LSB = 0xF8,
    BMP280_REG_PRESS_XLSB = 0xF9,
    BMP280_REG_TEMP_MSB = 0xFA,
    BMP280_REG_TEMP_LSB = 0xFB,
    BMP280_REG_TEMP_XLSB = 0xFC
} bmp280_reg_t;

void bmp280_model_register(void);

void bmp280_model_set_temperature(float temp_c);
void bmp280_model_set_pressure(float pressure_hpa);

float bmp280_model_get_temperature(void);
float bmp280_model_get_pressure(void);

#ifdef __cplusplus
}
#endif