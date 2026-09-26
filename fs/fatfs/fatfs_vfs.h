#pragma once

#include "vfs.h"
#include "hal_storage.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    hal_storage_t* storage;
    uint8_t pdrv;
    bool mounted;
} fatfs_ctx_t;

const vfs_ops_t* fatfs_get_ops(void);

int fatfs_mount(hal_storage_t* storage, uint8_t pdrv, const char* mount_point);
int fatfs_unmount(const char* mount_point);
int fatfs_format(hal_storage_t* storage, uint8_t pdrv, uint32_t au_size);
int fatfs_get_info(const char* mount_point, uint32_t* total, uint32_t* free);

#ifdef __cplusplus
}
#endif