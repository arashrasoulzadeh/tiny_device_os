#include "hal_spi.h"
#include "hal_power.h"
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>

#define SPI_MAX_DEVICES 8

typedef struct hal_spi {
    char path[32];
    spi_host_device_t host;
    hal_spi_config_t config;
    spi_device_handle_t devices[SPI_MAX_DEVICES];
    uint8_t device_count;
    bool initialized;
} hal_spi_t;

static spi_host_device_t hal_to_esp_host(const char* path) {
    if (strstr(path, "spi1") || strstr(path, "spi_1") || strstr(path, "spi_2")) {
        return SPI2_HOST;
    } else if (strstr(path, "spi3") || strstr(path, "spi_3")) {
        return SPI3_HOST;
    }
    return SPI2_HOST;
}

hal_spi_t* hal_spi_open(const char* path, const hal_spi_config_t* config) {
    hal_spi_t* spi = calloc(1, sizeof(hal_spi_t));
    if (!spi) return NULL;
    
    strncpy(spi->path, path, sizeof(spi->path) - 1);
    spi->host = hal_to_esp_host(path);
    
    if (config) {
        spi->config = *config;
    } else {
        spi->config.frequency = 1000000;
        spi->config.mode = HAL_SPI_MODE_0;
        spi->config.bit_order = HAL_SPI_BIT_ORDER_MSB;
        spi->config.bits_per_word = 8;
        spi->config.cs_active_high = false;
    }
    
    spi->device_count = 0;
    spi->initialized = false;
    
    return spi;
}

void hal_spi_close(hal_spi_t* spi) {
    if (!spi) return;
    for (uint8_t i = 0; i < spi->device_count; i++) {
        if (spi->devices[i]) {
            spi_bus_remove_device(spi->devices[i]);
        }
    }
    if (spi->initialized) {
        spi_bus_free(spi->host);
    }
    free(spi);
}

int hal_spi_init_bus(hal_spi_t* spi) {
    if (!spi || spi->initialized) return 0;
    
    spi_bus_config_t bus_config = {
        .mosi_io_num = (spi->host == SPI2_HOST) ? GPIO_NUM_23 : GPIO_NUM_13,
        .miso_io_num = (spi->host == SPI2_HOST) ? GPIO_NUM_19 : GPIO_NUM_12,
        .sclk_io_num = (spi->host == SPI2_HOST) ? GPIO_NUM_18 : GPIO_NUM_14,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    esp_err_t err = spi_bus_initialize(spi->host, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return -1;
    
    spi->initialized = true;
    return 0;
}

int hal_spi_add_device(hal_spi_t* spi, const hal_spi_config_t* config, uint8_t cs_gpio, spi_device_handle_t* handle) {
    if (!spi || spi->device_count >= SPI_MAX_DEVICES) return -1;
    if (!spi->initialized && hal_spi_init_bus(spi) != 0) return -1;
    
    spi_device_interface_config_t dev_config = {
        .mode = config ? config->mode : 0,
        .clock_speed_hz = config ? config->frequency : 1000000,
        .spics_io_num = cs_gpio,
        .queue_size = 4,
        .flags = config && config->cs_active_high ? SPI_DEVICE_POSITIVE_CS : 0,
    };
    
    esp_err_t err = spi_bus_add_device(spi->host, &dev_config, &spi->devices[spi->device_count]);
    if (err != ESP_OK) return -1;
    
    *handle = spi->devices[spi->device_count];
    spi->device_count++;
    return 0;
}

void hal_spi_close(hal_spi_t* spi) {
    if (!spi) return;
    for (uint8_t i = 0; i < spi->device_count; i++) {
        if (spi->devices[i]) {
            spi_bus_remove_device(spi->devices[i]);
        }
    }
    if (spi->initialized) {
        spi_bus_free(spi->host);
    }
    free(spi);
}

int hal_spi_transfer(hal_spi_t* spi, const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    if (!spi || len == 0) return -1;
    
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    
    esp_err_t err = spi_device_polling_transmit(spi->devices[0], &trans);
    return err == ESP_OK ? 0 : -1;
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
    // CS is handled automatically by SPI driver
}

void hal_spi_cs_deassert(hal_spi_t* spi) {
    // CS is handled automatically by SPI driver
}

const char* hal_spi_get_path(const hal_spi_t* spi) {
    return spi ? spi->path : NULL;
}