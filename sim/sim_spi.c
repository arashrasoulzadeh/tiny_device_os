#include "sim_spi.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t cs;
    bool registered;
    sim_spi_transfer_cb_t cb;
    void* arg;
} spi_device_t;

static spi_device_t g_devices[SIM_SPI_MAX_DEVICES];

int sim_spi_init(void) {
    memset(g_devices, 0, sizeof(g_devices));
    return 0;
}

void sim_spi_cleanup(void) {
}

int sim_spi_register_device(uint8_t cs, sim_spi_transfer_cb_t cb, void* arg) {
    for (int i = 0; i < SIM_SPI_MAX_DEVICES; i++) {
        if (!g_devices[i].registered) {
            g_devices[i].cs = cs;
            g_devices[i].cb = cb;
            g_devices[i].arg = arg;
            g_devices[i].registered = true;
            return 0;
        }
    }
    return -1;
}

void sim_spi_unregister_device(uint8_t cs) {
    for (int i = 0; i < SIM_SPI_MAX_DEVICES; i++) {
        if (g_devices[i].registered && g_devices[i].cs == cs) {
            g_devices[i].registered = false;
            return;
        }
    }
}

static spi_device_t* find_device(uint8_t cs) {
    for (int i = 0; i < SIM_SPI_MAX_DEVICES; i++) {
        if (g_devices[i].registered && g_devices[i].cs == cs) {
            return &g_devices[i];
        }
    }
    return NULL;
}

int sim_spi_transfer(uint8_t cs, const uint8_t* tx, uint8_t* rx, size_t len) {
    spi_device_t* dev = find_device(cs);
    if (!dev || !dev->cb) return -1;
    return dev->cb(cs, tx, rx, len, dev->arg);
}