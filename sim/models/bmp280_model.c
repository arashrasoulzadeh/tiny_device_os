#include "bmp280_model.h"
#include "sim_i2c.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    
    float temperature;
    float pressure;
    
    uint8_t ctrl_meas;
    uint8_t config;
} bmp280_t;

static bmp280_t g_bmp280 = {0};
static bool g_initialized = false;

static int bmp280_i2c_write(uint8_t addr, uint16_t reg, const uint8_t* data, size_t len, void* arg) {
    (void)arg;
    if (addr != BMP280_I2C_ADDR) return -1;
    if (len == 0) return 0;
    
    // The first byte is the register, rest are values
    if (len >= 1) {
        uint8_t value = data[0];
        
        switch (reg) {
            case BMP280_REG_RESET:
                if (value == 0xB6) {
                    bmp280_model_set_temperature(25.0f);
                    bmp280_model_set_pressure(1013.25f);
                    g_bmp280.ctrl_meas = 0;
                    g_bmp280.config = 0;
                }
                break;
            case BMP280_REG_CTRL_MEAS:
                g_bmp280.ctrl_meas = value;
                break;
            case BMP280_REG_CONFIG:
                g_bmp280.config = value;
                break;
        }
    }
    return 0;
}

static int bmp280_i2c_read(uint8_t addr, uint16_t reg, uint8_t* data, size_t len, void* arg) {
    (void)arg;
    if (addr != BMP280_I2C_ADDR || !data || len == 0) return -1;
    
    data[0] = 0;
    
    static uint8_t current_reg = 0;
    
    if (len == 1) {
        switch (current_reg) {
            case BMP280_REG_ID:
                data[0] = BMP280_CHIP_ID;
                break;
            case BMP280_REG_STATUS:
                data[0] = 0x00;
                break;
            case BMP280_REG_CTRL_MEAS:
                data[0] = g_bmp280.ctrl_meas;
                break;
            case BMP280_REG_CONFIG:
                data[0] = g_bmp280.config;
                break;
            case BMP280_REG_PRESS_MSB:
            case BMP280_REG_PRESS_LSB:
            case BMP280_REG_PRESS_XLSB:
            case BMP280_REG_TEMP_MSB:
            case BMP280_REG_TEMP_LSB:
            case BMP280_REG_TEMP_XLSB: {
                int32_t raw_temp = (int32_t)((g_bmp280.temperature + 45.0f) * 100.0f * 16.0f);
                int32_t raw_press = (int32_t)(g_bmp280.pressure * 256.0f);
                
                if (current_reg == BMP280_REG_PRESS_MSB) data[0] = (raw_press >> 12) & 0xFF;
                else if (current_reg == BMP280_REG_PRESS_LSB) data[0] = (raw_press >> 4) & 0xFF;
                else if (current_reg == BMP280_REG_PRESS_XLSB) data[0] = (raw_press << 4) & 0xFF;
                else if (current_reg == BMP280_REG_TEMP_MSB) data[0] = (raw_temp >> 12) & 0xFF;
                else if (current_reg == BMP280_REG_TEMP_LSB) data[0] = (raw_temp >> 4) & 0xFF;
                else if (current_reg == BMP280_REG_TEMP_XLSB) data[0] = (raw_temp << 4) & 0xFF;
                break;
            }
            case BMP280_REG_CALIB_00:
                data[0] = g_bmp280.dig_T1 & 0xFF;
                break;
            case BMP280_REG_CALIB_00 + 1:
                data[0] = (g_bmp280.dig_T1 >> 8) & 0xFF;
                break;
            case BMP280_REG_CALIB_00 + 2:
                data[0] = g_bmp280.dig_T2 & 0xFF;
                break;
            case BMP280_REG_CALIB_00 + 3:
                data[0] = (g_bmp280.dig_T2 >> 8) & 0xFF;
                break;
            case BMP280_REG_CALIB_00 + 4:
                data[0] = g_bmp280.dig_T3 & 0xFF;
                break;
            case BMP280_REG_CALIB_00 + 5:
                data[0] = (g_bmp280.dig_T3 >> 8) & 0xFF;
                break;
            case BMP280_REG_CALIB_00 + 6:
                data[0] = g_bmp280.dig_P1 & 0xFF;
                break;
            case BMP280_REG_CALIB_00 + 7:
                data[0] = (g_bmp280.dig_P1 >> 8) & 0xFF;
                break;
            default:
                break;
        }
    } else if (len > 1) {
        uint8_t reg = data[0];
        current_reg = reg;
        for (size_t i = 0; i < len; i++) {
            bmp280_i2c_read(addr, current_reg, &data[i], 1, arg);
            current_reg++;
        }
    }
    
    return 0;
}

void bmp280_model_register(void) {
    if (g_initialized) return;
    
    g_bmp280.dig_T1 = 27504;
    g_bmp280.dig_T2 = 26435;
    g_bmp280.dig_T3 = -1000;
    g_bmp280.dig_P1 = 36477;
    g_bmp280.dig_P2 = -10685;
    g_bmp280.dig_P3 = 3024;
    g_bmp280.dig_P4 = 2855;
    g_bmp280.dig_P5 = 140;
    g_bmp280.dig_P6 = -7;
    g_bmp280.dig_P7 = 15500;
    g_bmp280.dig_P8 = -14600;
    g_bmp280.dig_P9 = 6000;
    
    g_bmp280.temperature = 25.0f;
    g_bmp280.pressure = 1013.25f;
    g_bmp280.ctrl_meas = 0;
    g_bmp280.config = 0;
    
    sim_i2c_register_device(BMP280_I2C_ADDR, bmp280_i2c_read, bmp280_i2c_write, NULL);
    g_initialized = true;
}

void bmp280_model_set_temperature(float temp_c) {
    g_bmp280.temperature = temp_c;
}

void bmp280_model_set_pressure(float pressure_hpa) {
    g_bmp280.pressure = pressure_hpa;
}

float bmp280_model_get_temperature(void) {
    return g_bmp280.temperature;
}

float bmp280_model_get_pressure(void) {
    return g_bmp280.pressure;
}