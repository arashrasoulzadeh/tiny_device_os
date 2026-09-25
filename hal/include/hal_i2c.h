#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_I2C_SPEED_STANDARD = 100000,
    HAL_I2C_SPEED_FAST = 400000,
    HAL_I2C_SPEED_FAST_PLUS = 1000000,
    HAL_I2C_SPEED_HIGH = 3400000
} hal_i2c_speed_t;

typedef struct hal_i2c hal_i2c_t;

hal_i2c_t* hal_i2c_open(const char* path, hal_i2c_speed_t speed);
void hal_i2c_close(hal_i2c_t* i2c);

int hal_i2c_write(hal_i2c_t* i2c, uint8_t addr, const uint8_t* data, size_t len);
int hal_i2c_read(hal_i2c_t* i2c, uint8_t addr, uint8_t* data, size_t len);
int hal_i2c_write_read(hal_i2c_t* i2c, uint8_t addr, 
                        const uint8_t* write_data, size_t write_len,
                        uint8_t* read_data, size_t read_len);

int hal_i2c_mem_write(hal_i2c_t* i2c, uint8_t addr, uint16_t mem_addr, 
                      uint16_t mem_addr_size, const uint8_t* data, size_t len);
int hal_i2c_mem_read(hal_i2c_t* i2c, uint8_t addr, uint16_t mem_addr, 
                     uint16_t mem_addr_size, uint8_t* data, size_t len);

int hal_i2c_scan(hal_i2c_t* i2c, uint8_t* addrs, size_t max_addrs, size_t* found);

int hal_i2c_set_timeout(hal_i2c_t* i2c, uint32_t timeout_ms);
uint32_t hal_i2c_get_timeout(const hal_i2c_t* i2c);

const char* hal_i2c_get_path(const hal_i2c_t* i2c);

// Power management
int hal_i2c_suspend(hal_i2c_t* i2c);
int hal_i2c_resume(hal_i2c_t* i2c);

#ifdef __cplusplus
}
#endif