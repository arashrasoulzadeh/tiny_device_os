#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int sim_storage_init(const char* flash_image, const char* sd_image);
void sim_storage_cleanup(void);

int sim_storage_flash_read(uint32_t offset, void* buffer, size_t size);
int sim_storage_flash_write(uint32_t offset, const void* buffer, size_t size);
int sim_storage_flash_erase(uint32_t offset, size_t size);

int sim_storage_sd_read(uint32_t offset, void* buffer, size_t size);
int sim_storage_sd_write(uint32_t offset, const void* buffer, size_t size);

bool sim_storage_flash_exists(void);
bool sim_storage_sd_exists(void);

uint32_t sim_storage_flash_size(void);
uint32_t sim_storage_sd_size(void);

#ifdef __cplusplus
}
#endif