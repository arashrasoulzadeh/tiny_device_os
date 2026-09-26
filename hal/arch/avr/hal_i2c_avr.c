#include "hal_i2c.h"
#include "hal_power.h"
#include <avr/io.h>
#include <util/twi.h>
#include <string.h>
#include <stdlib.h>

typedef struct hal_i2c {
    char path[32];
    uint32_t speed;
    uint32_t timeout_ms;
    bool initialized;
} hal_i2c_t;

static inline void i2c_init(hal_i2c_t* i2c) {
    TWSR = 0;
    TWBR = ((F_CPU / i2c->speed) - 16) / 2;
}

hal_i2c_t* hal_i2c_open(const char* path, hal_i2c_speed_t speed) {
    hal_i2c_t* i2c = calloc(1, sizeof(hal_i2c_t));
    if (!i2c) return NULL;
    
    strncpy(i2c->path, path, sizeof(i2c->path) - 1);
    i2c->speed = speed;
    i2c->timeout_ms = 1000;
    i2c->initialized = false;
    
    return i2c;
}

void hal_i2c_close(hal_i2c_t* i2c) {
    if (!i2c) return;
    if (i2c->initialized) {
        TWCR = 0;
    }
    free(i2c);
}

int hal_i2c_write(hal_i2c_t* i2c, uint8_t addr, const uint8_t* data, size_t len) {
    if (!i2c || !data || len == 0) return -1;
    if (!i2c->initialized) {
        TWSR = 0;
        TWBR = ((F_CPU / i2c->speed) - 16) / 2;
        i2c->initialized = true;
    }
    
    TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    if ((TWSR & 0xF8) != TW_START) return -1;
    
    TWDR = (addr << 1) | TW_WRITE;
    TWCR = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    if ((TWSR & 0xF8) != TW_MT_SLA_ACK) return -1;
    
    for (size_t i = 0; i < len; i++) {
        TWDR = data[i];
        TWCR = (1<<TWINT) | (1<<TWEN);
        while (!(TWCR & (1<<TWINT)));
        if ((TWSR & 0xF8) != TW_MT_DATA_ACK) return -1;
    }
    
    TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
    return 0;
}

int hal_i2c_read(hal_i2c_t* i2c, uint8_t addr, uint8_t* data, size_t len) {
    if (!i2c || !data || len == 0) return -1;
    if (!i2c->initialized) {
        TWSR = 0;
        TWBR = ((F_CPU / i2c->speed) - 16) / 2;
        i2c->initialized = true;
    }
    
    TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    if ((TWSR & 0xF8) != TW_START) return -1;
    
    TWDR = (addr << 1) | TW_READ;
    TWCR = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    if ((TWSR & 0xF8) != TW_MR_SLA_ACK) return -1;
    
    for (size_t i = 0; i < len; i++) {
        if (i == len - 1) {
            TWCR = (1<<TWINT) | (1<<TWEN);
        } else {
            TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
        }
        while (!(TWCR & (1<<TWINT)));
        data[i] = TWDR;
    }
    
    TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
    return 0;
}

int hal_i2c_write_read(hal_i2c_t* i2c, uint8_t addr,
                       const uint8_t* write_data, size_t write_len,
                       uint8_t* read_data, size_t read_len) {
    if (!i2c) return -1;
    if (!i2c->initialized) {
        TWSR = 0;
        TWBR = ((F_CPU / i2c->speed) - 16) / 2;
        i2c->initialized = true;
    }
    
    TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    if ((TWSR & 0xF8) != TW_START) return -1;
    
    TWDR = (addr << 1) | TW_WRITE;
    TWCR = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    if ((TWSR & 0xF8) != TW_MT_SLA_ACK) return -1;
    
    for (size_t i = 0; i < write_len; i++) {
        TWDR = write_data[i];
        TWCR = (1<<TWINT) | (1<<TWEN);
        while (!(TWCR & (1<<TWINT)));
        if ((TWSR & 0xF8) != TW_MT_DATA_ACK) return -1;
    }
    
    TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    if ((TWSR & 0xF8) != TW_REP_START) return -1;
    
    TWDR = (addr << 1) | TW_READ;
    TWCR = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    if ((TWSR & 0xF8) != TW_MR_SLA_ACK) return -1;
    
    for (size_t i = 0; i < read_len; i++) {
        if (i == read_len - 1) {
            TWCR = (1<<TWINT) | (1<<TWEN);
        } else {
            TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
        }
        while (!(TWCR & (1<<TWINT)));
        if ((TWSR & 0xF8) != (i == read_len - 1 ? TW_MR_DATA_NACK : TW_MR_DATA_ACK)) return -1;
        read_data[i] = TWDR;
    }
    
    TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
    return 0;
}

int hal_i2c_mem_write(hal_i2c_t* i2c, uint8_t addr, uint16_t mem_addr,
                      uint16_t mem_addr_size, const uint8_t* data, size_t len) {
    (void)i2c; (void)addr; (void)mem_addr; (void)mem_addr_size; (void)data; (void)len;
    return -1;
}

int hal_i2c_mem_read(hal_i2c_t* i2c, uint8_t addr, uint16_t mem_addr,
                     uint16_t mem_addr_size, uint8_t* data, size_t len) {
    (void)i2c; (void)addr; (void)mem_addr; (void)mem_addr_size; (void)data; (void)len;
    return -1;
}

int hal_i2c_scan(hal_i2c_t* i2c, uint8_t* addrs, size_t max_addrs, size_t* found) {
    if (!i2c || !addrs || !found) return -1;
    if (!i2c->initialized) {
        TWSR = 0;
        TWBR = ((F_CPU / i2c->speed) - 16) / 2;
        i2c->initialized = true;
    }
    
    size_t count = 0;
    for (uint8_t addr = 0x08; addr <= 0x77 && count < max_addrs; addr++) {
        TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
        while (!(TWCR & (1<<TWINT)));
        if ((TWSR & 0xF8) != TW_START) continue;
        
        TWDR = (addr << 1) | TW_WRITE;
        TWCR = (1<<TWINT) | (1<<TWEN);
        while (!(TWCR & (1<<TWINT)));
        
        if ((TWSR & 0xF8) == TW_MT_SLA_ACK) {
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
    TWCR = 0;
    i2c->initialized = false;
    return 0;
}

int hal_i2c_resume(hal_i2c_t* i2c) {
    if (!i2c) return -1;
    TWSR = 0;
    TWBR = ((F_CPU / i2c->speed) - 16) / 2;
    i2c->initialized = true;
    return 0;
}