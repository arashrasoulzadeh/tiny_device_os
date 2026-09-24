#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_SPI_MAX_DEVICES 8

typedef int (*sim_spi_transfer_cb_t)(uint8_t cs, const uint8_t* tx, uint8_t* rx, size_t len, void* arg);

int sim_spi_init(void);
void sim_spi_cleanup(void);

int sim_spi_register_device(uint8_t cs, sim_spi_transfer_cb_t cb, void* arg);
void sim_spi_unregister_device(uint8_t cs);

int sim_spi_transfer(uint8_t cs, const uint8_t* tx, uint8_t* rx, size_t len);

#ifdef __cplusplus
}
#endif