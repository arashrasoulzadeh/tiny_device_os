#include "hal_storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SIM_STORAGE_SIZE (4 * 1024 * 1024)

struct hal_storage {
    char path[64];
    hal_storage_type_t type;
    uint8_t* data;
    size_t size;
    hal_storage_callback_t callback;
    void* callback_arg;
    bool initialized;
};

hal_storage_t* hal_storage_open(const char* path, hal_storage_type_t type) {
    hal_storage_t* storage = calloc(1, sizeof(hal_storage_t));
    if (!storage) return NULL;
    
    strncpy(storage->path, path, sizeof(storage->path) - 1);
    storage->type = type;
    storage->size = SIM_STORAGE_SIZE;
    storage->data = calloc(1, SIM_STORAGE_SIZE);
    storage->initialized = false;
    
    return storage;
}

void hal_storage_close(hal_storage_t* storage) {
    if (storage) {
        free(storage->data);
        free(storage);
    }
}

int hal_storage_init(hal_storage_t* storage) {
    if (!storage) return -1;
    storage->initialized = true;
    return 0;
}

int hal_storage_deinit(hal_storage_t* storage) {
    if (!storage) return -1;
    storage->initialized = false;
    return 0;
}

int hal_storage_read(hal_storage_t* storage, uint32_t offset, void* buffer, size_t size) {
    if (!storage || !storage->initialized || !buffer) return -1;
    if (offset + size > storage->size) return -1;
    
    memcpy(buffer, storage->data + offset, size);
    return 0;
}

int hal_storage_write(hal_storage_t* storage, uint32_t offset, const void* buffer, size_t size) {
    if (!storage || !storage->initialized || !buffer) return -1;
    if (offset + size > storage->size) return -1;
    
    memcpy(storage->data + offset, buffer, size);
    
    if (storage->callback) {
        storage->callback(storage, HAL_STORAGE_OP_WRITE, 0, storage->callback_arg);
    }
    return 0;
}

int hal_storage_erase(hal_storage_t* storage, uint32_t offset, size_t size) {
    if (!storage || !storage->initialized) return -1;
    if (offset + size > storage->size) return -1;
    
    memset(storage->data + offset, 0xFF, size);
    
    if (storage->callback) {
        storage->callback(storage, HAL_STORAGE_OP_ERASE, 0, storage->callback_arg);
    }
    return 0;
}

int hal_storage_sync(hal_storage_t* storage) {
    if (!storage) return -1;
    
    if (storage->callback) {
        storage->callback(storage, HAL_STORAGE_OP_SYNC, 0, storage->callback_arg);
    }
    return 0;
}

int hal_storage_get_info(hal_storage_t* storage, hal_storage_info_t* info) {
    if (!storage || !info) return -1;
    
    info->total_bytes = storage->size;
    info->used_bytes = 0;
    info->free_bytes = storage->size;
    info->block_size = 4096;
    info->page_size = 256;
    
    return 0;
}

int hal_storage_set_callback(hal_storage_t* storage, hal_storage_callback_t cb, void* arg) {
    if (!storage) return -1;
    storage->callback = cb;
    storage->callback_arg = arg;
    return 0;
}

const char* hal_storage_get_path(const hal_storage_t* storage) {
    return storage ? storage->path : NULL;
}

hal_storage_type_t hal_storage_get_type(const hal_storage_t* storage) {
    return storage ? storage->type : HAL_STORAGE_TYPE_FLASH;
}

int hal_storage_suspend(hal_storage_t* storage) {
    if (!storage) return -1;
    storage->initialized = false;
    return 0;
}

int hal_storage_resume(hal_storage_t* storage) {
    if (!storage) return -1;
    storage->initialized = true;
    return 0;
}