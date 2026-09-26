#include "hal_spi.h"
#include "hal_power.h"
#include <avr/io.h>
#include <string.h>
#include <stdlib.h>

typedef struct hal_spi {
    char path[32];
    hal_spi_config_t config;
    bool initialized;
} hal_spi_t;

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
    
    spi->initialized = false;
    return spi;
}

void hal_spi_close(hal_spi_t* spi) {
    if (!spi) return;
    if (spi->initialized) {
        SPCR = 0;
    }
    free(spi);
}

int hal_spi_init(hal_spi_t* spi) {
    if (!spi || spi->initialized) return -1;
    
    DDRB |= (1<<PB3) | (1<<PB5) | (1<<PB2); // MOSI, SCK, SS as output
    DDRB &= ~(1<<PB4); // MISO as input
    
    uint8_t spr = 0;
    if (spi->config.frequency <= F_CPU / 128) SPCR |= (1<<SPR0) | (1<<SPR1);
    else if (spi->config.frequency <= F_CPU / 64) SPCR |= (1<<SPR1);
    else if (spi->config.frequency <= F_CPU / 16) SPCR |= (1<<SPR0);
    
    if (spi->config.mode & 0x01) SPCR |= (1<<CPHA);
    if (spi->config.mode & 0x02) SPCR |= (1<<CPOL);
    
    if (spi->config.bit_order == HAL_SPI_BIT_ORDER_MSB) {
        SPCR &= ~(1<<DORD);
    } else {
        SPCR |= (1<<DORD);
    }
    
    SPCR |= (1<<SPE) | (1<<MSTR);
    
    spi->initialized = true;
    return 0;
}

int hal_spi_transfer(hal_spi_t* spi, const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    if (!spi || len == 0) return -1;
    if (!spi->initialized) return -1;
    
    for (size_t i = 0; i < len; i++) {
        SPDR = tx_data ? tx_data[i] : 0xFF;
        while (!(SPSR & (1<<SPIF)));
        if (rx_data) rx_data[i] = SPDR;
    }
    return 0;
}

int hal_spi_write(hal_spi_t* spi, const uint8_t* data, size_t len) {
    return hal_spi_transfer(spi, data, NULL, len);
}

int hal_spi_read(hal_spi_t* spi, uint8_t* data, size_t len) {
    return hal_spi_transfer(spi, NULL, data, len);
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
    // CS pin controlled by user
}

void hal_spi_cs_deassert(hal_spi_t* spi) {
    // CS pin controlled by user
}

const char* hal_spi_get_path(const hal_spi_t* spi) {
    return spi ? spi->path : NULL;
}

int hal_spi_suspend(hal_spi_t* spi) {
    if (!spi) return -1;
    SPCR = 0;
    return 0;
}

int hal_spi_resume(hal_spi_t* spi) {
    if (!spi) return -1;
    return 0;
}