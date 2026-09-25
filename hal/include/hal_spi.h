#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_SPI_MODE_0 = 0,
    HAL_SPI_MODE_1,
    HAL_SPI_MODE_2,
    HAL_SPI_MODE_3
} hal_spi_mode_t;

typedef enum {
    HAL_SPI_BIT_ORDER_MSB = 0,
    HAL_SPI_BIT_ORDER_LSB
} hal_spi_bit_order_t;

typedef struct hal_spi hal_spi_t;

typedef struct {
    uint32_t frequency;
    hal_spi_mode_t mode;
    hal_spi_bit_order_t bit_order;
    uint8_t bits_per_word;
    bool cs_active_high;
} hal_spi_config_t;

hal_spi_t* hal_spi_open(const char* path, const hal_spi_config_t* config);
void hal_spi_close(hal_spi_t* spi);

int hal_spi_transfer(hal_spi_t* spi, const uint8_t* tx_data, uint8_t* rx_data, size_t len);
int hal_spi_write(hal_spi_t* spi, const uint8_t* data, size_t len);
int hal_spi_read(hal_spi_t* spi, uint8_t* data, size_t len);

int hal_spi_set_config(hal_spi_t* spi, const hal_spi_config_t* config);
int hal_spi_get_config(hal_spi_t* spi, hal_spi_config_t* config);

void hal_spi_cs_assert(hal_spi_t* spi);
void hal_spi_cs_deassert(hal_spi_t* spi);

const char* hal_spi_get_path(const hal_spi_t* spi);

// Power management
int hal_spi_suspend(hal_spi_t* spi);
int hal_spi_resume(hal_spi_t* spi);

#ifdef __cplusplus
}
#endif