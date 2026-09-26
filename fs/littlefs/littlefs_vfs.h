#pragma once

#include "hal_storage.h"
#include "lfs.h"
#include "vfs.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  hal_storage_t *storage;
  uint32_t offset;
  uint32_t size;
  uint32_t block_size;
  uint32_t read_size;
  uint32_t prog_size;
  uint32_t lookahead_size;
  struct lfs_config *lfs_cfg;
  lfs_t lfs;
} lfs_fs_ctx_t;

const vfs_ops_t *littlefs_get_ops(void);

int littlefs_mount(hal_storage_t *storage, uint32_t offset, uint32_t size,
                   uint32_t block_size, const char *mount_point);
int littlefs_unmount(const char *mount_point);
int littlefs_format(hal_storage_t *storage, uint32_t offset, uint32_t size,
                    uint32_t block_size);
int littlefs_get_info(const char *mount_point, uint32_t *total, uint32_t *used);

#ifdef __cplusplus
}
#endif