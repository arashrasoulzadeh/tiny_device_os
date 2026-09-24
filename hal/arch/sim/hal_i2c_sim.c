#include "hal_i2c.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct hal_i2c {
    char path[64];
    uint32_t speed;
    uint32_t timeout_ms;
    uint8_t slave_addrs[128];
    uint8_t slave_count;
};

hal_i2c_t* hal_i2c_open(const char* path, hal_i2c_speed_t speed) {
    hal_i2c_t* i2c = calloc(1, sizeof(hal_i2c_t));
    if (!i2c) return NULL;
    
    strncpy(i2c->path, path, sizeof(i2c->path) - 1);
    i2c->speed = speed;
    i2c->timeout_ms = 1000;
    
    return i2c;
}

void hal_i2c_close(hal_i2c_t* i2c) {
    if (i2c) free(i2c);
}

int hal_i2c_write(hal_i2c_t* i2c, uint8_t addr, const uint8_t* data, size_t len) {
    (void)i2c; (void)addr; (void)data; (void)len;
    return 0;
}

int hal_i2c_read(hal_i2c_t* i2c, uint8_t addr, uint8_t* data, size_t len) {
    (void)i2c; (void)addr; (void)data; (void)len;
    return 0;
}

int hal_i2c_write_read(hal_i2c_t* i2c, uint8_t addr,
                       const uint8_t* write_data, size_t write_len,
                       uint8_t* read_data, size_t read_len) {
    (void)i2c; (void)addr; (void)write_data; (void)write_len; (void)read_data; (void)read_len;
    return 0;
}

int hal_i2c_mem_write(hal_i2c_t* i2c, uint8_t addr, uint16_t mem_addr,
                      uint16_t mem_addr_size, const uint8_t* data, size_t len) {
    (void)i2c; (void)addr; (void)mem_addr; (void)mem_addr_size; (void)data; (void)len;
    return 0;
}

int hal_i2c_mem_read(hal_i2c_t* i2c, uint8_t addr, uint16_t mem_addr,
                     uint16_t mem_addr_size, uint8_t* data, size_t len) {
    (void)i2c; (void)addr; (void)mem_addr; (void)mem_addr_size; (void)data; (void)len;
    return 0;
}

int hal_i2c_scan(hal_i2c_t* i2c, uint8_t* addrs, size_t max_addrs, size_t* found) {
    (void)i2c; (void)addrs; (void)max_addrs;
    if (found) *found = 0;
    return 0;
}

int hal_i2c_set_timeout(hal_i2c_t* i2c, uint32_t timeout_ms) {
    if (!i2c) return -1;
    i2c->timeout_ms = timeout_ms;
    return 0;
}

uint32_t hal_i2c_get_timeout(const hal_i2c_t* i2c) {
    return i2c ? i2c->timeout_ms : 0;
}

const char* hal_i2c_get_path(const hal_i2c_t* i2c) {
    return i2c ? i2c->path : NULL;
}