// hal_i2c_* implementation for ESP8266 Arduino framework.
// Uses Arduino Wire library instead of ESP-IDF driver/i2c.h.
#include "hal_i2c.h"
#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>
#include <string.h>

struct hal_i2c {
  char path[32];
  int sda;
  int scl;
  uint32_t freq;
  bool initialized;
};

extern "C" {

hal_i2c_t *hal_i2c_open(const char *path, hal_i2c_speed_t speed) {
  hal_i2c_t *i2c = (hal_i2c_t *)calloc(1, sizeof(hal_i2c_t));
  if (!i2c)
    return NULL;

  strncpy(i2c->path, path, sizeof(i2c->path) - 1);
  // Default pins from device_config.yaml (D1=GPIO5=SCL, D2=GPIO4=SDA)
  i2c->sda = 4;
  i2c->scl = 5;
  i2c->freq = (uint32_t)speed;
  i2c->initialized = false;

  return i2c;
}

void hal_i2c_close(hal_i2c_t *i2c) {
  if (!i2c)
    return;
  free(i2c);
}

int hal_i2c_init(hal_i2c_t *i2c) {
  if (!i2c || i2c->initialized)
    return -1;

  Wire.begin(i2c->sda, i2c->scl);
  Wire.setClock(i2c->freq);
  i2c->initialized = true;
  return 0;
}

int hal_i2c_deinit(hal_i2c_t *i2c) {
  if (!i2c || !i2c->initialized)
    return -1;
  i2c->initialized = false;
  return 0;
}

int hal_i2c_write(hal_i2c_t *i2c, uint8_t addr, const uint8_t *data,
                  size_t len) {
  if (!i2c || !i2c->initialized || !data)
    return -1;

  Wire.beginTransmission(addr);
  Wire.write(data, len);
  uint8_t result = Wire.endTransmission();
  return result == 0 ? 0 : -1;
}

int hal_i2c_read(hal_i2c_t *i2c, uint8_t addr, uint8_t *data, size_t len) {
  if (!i2c || !i2c->initialized || !data)
    return -1;

  uint8_t received = Wire.requestFrom(addr, len);
  if (received != len)
    return -1;
  for (size_t i = 0; i < len; i++) {
    data[i] = Wire.read();
  }
  return 0;
}

int hal_i2c_write_read(hal_i2c_t *i2c, uint8_t addr, const uint8_t *write_data,
                       size_t write_len, uint8_t *read_data, size_t read_len) {
  if (!i2c || !i2c->initialized)
    return -1;

  // Write phase
  Wire.beginTransmission(addr);
  if (write_data && write_len > 0) {
    Wire.write(write_data, write_len);
  }
  uint8_t result = Wire.endTransmission(false); // false = repeated start
  if (result != 0)
    return -1;

  // Read phase
  uint8_t received = Wire.requestFrom(addr, read_len);
  if (received != read_len)
    return -1;
  for (size_t i = 0; i < read_len; i++) {
    read_data[i] = Wire.read();
  }
  return 0;
}

int hal_i2c_mem_write(hal_i2c_t *i2c, uint8_t addr, uint16_t mem_addr,
                      uint16_t mem_addr_size, const uint8_t *data, size_t len) {
  if (!i2c || !i2c->initialized || !data)
    return -1;

  Wire.beginTransmission(addr);
  if (mem_addr_size == 2) {
    Wire.write((uint8_t)(mem_addr >> 8));
    Wire.write((uint8_t)(mem_addr & 0xFF));
  } else {
    Wire.write((uint8_t)mem_addr);
  }
  Wire.write(data, len);
  uint8_t result = Wire.endTransmission();
  return result == 0 ? 0 : -1;
}

int hal_i2c_mem_read(hal_i2c_t *i2c, uint8_t addr, uint16_t mem_addr,
                     uint16_t mem_addr_size, uint8_t *data, size_t len) {
  if (!i2c || !i2c->initialized || !data)
    return -1;

  Wire.beginTransmission(addr);
  if (mem_addr_size == 2) {
    Wire.write((uint8_t)(mem_addr >> 8));
    Wire.write((uint8_t)(mem_addr & 0xFF));
  } else {
    Wire.write((uint8_t)mem_addr);
  }
  uint8_t result = Wire.endTransmission(false);
  if (result != 0)
    return -1;

  uint8_t received = Wire.requestFrom(addr, len);
  if (received != len)
    return -1;
  for (size_t i = 0; i < len; i++) {
    data[i] = Wire.read();
  }
  return 0;
}

int hal_i2c_scan(hal_i2c_t *i2c, uint8_t *addrs, size_t max_addrs,
                 size_t *found) {
  if (!i2c || !i2c->initialized || !addrs || !found)
    return -1;

  *found = 0;
  for (uint8_t addr = 1; addr < 127 && *found < max_addrs; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      addrs[(*found)++] = addr;
    }
  }
  return 0;
}

int hal_i2c_set_timeout(hal_i2c_t *i2c, uint32_t timeout_ms) {
  (void)i2c;
  (void)timeout_ms;
  // Wire doesn't have configurable timeout in Arduino
  return 0;
}

uint32_t hal_i2c_get_timeout(const hal_i2c_t *i2c) {
  (void)i2c;
  return 1000; // Default timeout
}

const char *hal_i2c_get_path(const hal_i2c_t *i2c) {
  return i2c ? i2c->path : NULL;
}

int hal_i2c_suspend(hal_i2c_t *i2c) {
  (void)i2c;
  return 0;
}

int hal_i2c_resume(hal_i2c_t *i2c) {
  (void)i2c;
  return 0;
}

} // extern "C"
