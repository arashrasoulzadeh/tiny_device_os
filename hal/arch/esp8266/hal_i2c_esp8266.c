#include "hal_i2c.h"
#include "hal_power.h"
#include <driver/i2c.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>

#define I2C_TIMEOUT_MS_DEFAULT 1000

typedef struct hal_i2c {
    char path[32];
    i2c_port_t port;
    hal_i2c_speed_t speed;
    uint32_t timeout_ms;
    bool initialized;
} hal_i2c_t;

hal_i2c_t* hal_i2c_open(const char* path, hal_i2c_speed_t speed) {
    i2c_port_t port = I2C_NUM_0;
    if (strstr(path, "i2c1") || strstr(path, "i2c_1")) {
        port = I2C_NUM_1;
    }
    
    hal_i2c_t* i2c = calloc(1, sizeof(hal_i2c_t));
    if (!i2c) return NULL;
    
    strncpy(i2c->path, path, sizeof(i2c->path) - 1);
    i2c->port = port;
    i2c->speed = speed;
    i2c->timeout_ms = 1000;
    i2c->initialized = false;
    
    return i2c;
}

void hal_i2c_close(hal_i2c_t* i2c) {
    if (!i2c) return;
    if (i2c->initialized) {
        i2c_driver_delete(i2c->port);
    }
    free(i2c);
}

int hal_i2c_init_bus(hal_i2c_t* i2c) {
    if (!i2c || i2c->initialized) return 0;
    
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (i2c->port == I2C_NUM_0) ? GPIO_NUM_4 : GPIO_NUM_2,
        .scl_io_num = (i2c->port == I2C_NUM_0) ? GPIO_NUM_5 : GPIO_NUM_14,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c->speed,
    };
    
    esp_err_t err = i2c_param_config(i2c->port, &config);
    if (err != ESP_OK) return -1;
    
    err = i2c_driver_install(i2c->port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) return -1;
    
    i2c->initialized = true;
    return 0;
}

int hal_i2c_write(hal_i2c_t* i2c, uint8_t addr, const uint8_t* data, size_t len) {
    if (!i2c || !data || len == 0) return -1;
    if (!i2c->initialized && hal_i2c_init_bus(i2c) != 0) return -1;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (uint8_t*)data, len, true);
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(i2c->port, cmd, pdMS_TO_TICKS(i2c->timeout_ms));
    i2c_cmd_link_delete(cmd);
    
    return err == ESP_OK ? 0 : -1;
}

int hal_i2c_read(hal_i2c_t* i2c, uint8_t addr, uint8_t* data, size_t len) {
    if (!i2c || !data || len == 0) return -1;
    if (!i2c->initialized && hal_i2c_init_bus(i2c) != 0) return -1;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(i2c->port, cmd, pdMS_TO_TICKS(i2c->timeout_ms));
    i2c_cmd_link_delete(cmd);
    
    return err == ESP_OK ? 0 : -1;
}

int hal_i2c_write_read(hal_i2c_t* i2c, uint8_t addr,
                       const uint8_t* write_data, size_t write_len,
                       uint8_t* read_data, size_t read_len) {
    if (!i2c) return -1;
    if (!i2c->initialized && hal_i2c_init_bus(i2c) != 0) return -1;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    if (write_len > 0) {
        i2c_master_write(cmd, (uint8_t*)write_data, write_len, true);
    }
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (read_len > 1) {
        i2c_master_read(cmd, read_data, read_len - 1, I2C_MASTER_ACK);
    }
    if (read_len > 0) {
        i2c_master_read_byte(cmd, read_data + read_len - 1, I2C_MASTER_NACK);
    }
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(i2c->port, cmd, pdMS_TO_TICKS(i2c->timeout_ms));
    i2c_cmd_link_delete(cmd);
    
    return err == ESP_OK ? 0 : -1;
}

int hal_i2c_mem_write(hal_i2c_t* i2c, uint8_t addr, uint16_t mem_addr,
                      uint16_t mem_addr_size, const uint8_t* data, size_t len) {
    if (!i2c || !data || len == 0) return -1;
    if (!i2c->initialized && hal_i2c_init_bus(i2c) != 0) return -1;
    
    uint8_t* buf = malloc(len + mem_addr_size);
    if (!buf) return -1;
    
    if (mem_addr_size == 2) {
        buf[0] = (mem_addr >> 8) & 0xFF;
        buf[1] = mem_addr & 0xFF;
    } else {
        buf[0] = mem_addr & 0xFF;
    }
    memcpy(buf + mem_addr_size, data, len);
    
    int ret = hal_i2c_write(i2c, addr, buf, len + mem_addr_size);
    free(buf);
    return ret;
}

int hal_i2c_mem_read(hal_i2c_t* i2c, uint8_t addr, uint16_t mem_addr,
                     uint16_t mem_addr_size, uint8_t* data, size_t len) {
    if (!i2c || !data || len == 0) return -1;
    if (!i2c->initialized && hal_i2c_init_bus(i2c) != 0) return -1;
    
    uint8_t addr_buf[2];
    if (mem_addr_size == 2) {
        addr_buf[0] = (mem_addr >> 8) & 0xFF;
        addr_buf[1] = mem_addr & 0xFF;
    } else {
        addr_buf[0] = mem_addr & 0xFF;
    }
    
    return hal_i2c_write_read(i2c, addr, addr_buf, mem_addr_size, data, len);
}

int hal_i2c_scan(hal_i2c_t* i2c, uint8_t* addrs, size_t max_addrs, size_t* found) {
    if (!i2c || !addrs || !found) return -1;
    if (!i2c->initialized && hal_i2c_init_bus(i2c) != 0) return -1;
    
    size_t count = 0;
    for (uint8_t addr = 0x08; addr <= 0x77 && count < max_addrs; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t err = i2c_master_cmd_begin(i2c->port, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);
        
        if (err == ESP_OK) {
            addrs[count++] = addr;
        }
    }
    
    *found = count;
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

int hal_i2c_suspend(hal_i2c_t* i2c) {
    if (!i2c) return -1;
    if (i2c->initialized) {
        i2c_driver_delete(i2c->port);
        i2c->initialized = false;
    }
    return 0;
}

int hal_i2c_resume(hal_i2c_t* i2c) {
    if (!i2c) return -1;
    return hal_i2c_init_bus(i2c);
}