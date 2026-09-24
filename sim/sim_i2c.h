#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_I2C_MAX_DEVICES 16

typedef int (*sim_i2c_read_cb_t)(uint8_t addr, uint16_t reg, uint8_t* data, size_t len, void* arg);
typedef int (*sim_i2c_write_cb_t)(uint8_t addr, uint16_t reg, const uint8_t* data, size_t len, void* arg);

int sim_i2c_init(void);
void sim_i2c_cleanup(void);

int sim_i2c_register_device(uint8_t addr, sim_i2c_read_cb_t read_cb, sim_i2c_write_cb_t write_cb, void* arg);
void sim_i2c_unregister_device(uint8_t addr);

int sim_i2c_write(uint8_t addr, const uint8_t* data, size_t len);
int sim_i2c_read(uint8_t addr, uint8_t* data, size_t len);
int sim_i2c_write_read(uint8_t addr, const uint8_t* write_data, size_t write_len, uint8_t* read_data, size_t read_len);

#ifdef __cplusplus
}
#endif