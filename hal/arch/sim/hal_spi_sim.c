#include "hal_spi.h"
#include <stdlib.h>
#include <string.h>

struct hal_spi {
    char path[64];
    hal_spi_config_t config;
    bool cs_active;
};

hal_spi_t* hal_spi_open(const char* path, const hal_spi_config_t* config) {
    hal_spi_t* spi = calloc(1, sizeof(hal_spi_t));
    if (!spi) return NULL;
    
    strncpy(spi->path, path, sizeof(spi->path) - 1);
    if (config) {
        spi->config = *config;
    } else {
        spi->config.frequency = 1000000;
        spi->config.mode = HAL_SPI_MODE_0;
        spi->config.bit_order = HAL_SPI_BIT_ORDER_MSB;
        spi->config.bits_per_word = 8;
        spi->config.cs_active_high = false;
    }
    spi->cs_active = false;
    
    return spi;
}

void hal_spi_close(hal_spi_t* spi) {
    if (spi) free(spi);
}

int hal_spi_transfer(hal_spi_t* spi, const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    (void)spi; (void)tx_data; (void)rx_data; (void)len;
    return 0;
}

int hal_spi_write(hal_spi_t* spi, const uint8_t* data, size_t len) {
    (void)spi; (void)data; (void)len;
    return 0;
}

int hal_spi_read(hal_spi_t* spi, uint8_t* data, size_t len) {
    (void)spi; (void)data; (void)len;
    return 0;
}

int hal_spi_set_config(hal_spi_t* spi, const hal_spi_config_t* config) {
    if (!spi || !config) return -1;
    spi->config = *config;
    return 0;
}

int hal_spi_get_config(hal_spi_t* spi, hal_spi_config_t* config) {
    if (!spi || !config) return -1;
    *config = spi->config;
    return 0;
}

void hal_spi_cs_assert(hal_spi_t* spi) {
    if (spi) spi->cs_active = true;
}

void hal_spi_cs_deassert(hal_spi_t* spi) {
    if (spi) spi->cs_active = false;
}

const char* hal_spi_get_path(const hal_spi_t* spi) {
    return spi ? spi->path : NULL;
}