#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_STORAGE_TYPE_FLASH = 0,
    HAL_STORAGE_TYPE_SD_SPI,
    HAL_STORAGE_TYPE_SD_SDIO,
    HAL_STORAGE_TYPE_RAM
} hal_storage_type_t;

typedef struct hal_storage hal_storage_t;

typedef struct {
    uint32_t total_bytes;
    uint32_t free_bytes;
    uint32_t used_bytes;
    uint16_t block_size;
    uint16_t page_size;
} hal_storage_info_t;

typedef enum {
    HAL_STORAGE_OP_READ = 0,
    HAL_STORAGE_OP_WRITE,
    HAL_STORAGE_OP_ERASE,
    HAL_STORAGE_OP_SYNC
} hal_storage_op_t;

typedef void (*hal_storage_callback_t)(hal_storage_t* storage, hal_storage_op_t op, 
                                        int result, void* arg);

hal_storage_t* hal_storage_open(const char* path, hal_storage_type_t type);
void hal_storage_close(hal_storage_t* storage);

int hal_storage_init(hal_storage_t* storage);
int hal_storage_deinit(hal_storage_t* storage);

int hal_storage_read(hal_storage_t* storage, uint32_t offset, void* buffer, size_t size);
int hal_storage_write(hal_storage_t* storage, uint32_t offset, const void* buffer, size_t size);
int hal_storage_erase(hal_storage_t* storage, uint32_t offset, size_t size);
int hal_storage_sync(hal_storage_t* storage);

int hal_storage_get_info(hal_storage_t* storage, hal_storage_info_t* info);

int hal_storage_set_callback(hal_storage_t* storage, hal_storage_callback_t cb, void* arg);

const char* hal_storage_get_path(const hal_storage_t* storage);
hal_storage_type_t hal_storage_get_type(const hal_storage_t* storage);

#ifdef __cplusplus
}
#endif