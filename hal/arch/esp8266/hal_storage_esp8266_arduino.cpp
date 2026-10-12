// hal_storage_* implementation for ESP8266 Arduino framework.
// Uses Arduino LittleFS library instead of ESP-IDF esp_vfs_littlefs.
#include "hal_storage.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <stdlib.h>
#include <string.h>

struct hal_storage {
  char path[32];
  hal_storage_type_t type;
  bool initialized;
  char mount_point[32];
};

extern "C" {

hal_storage_t *hal_storage_open(const char *path, hal_storage_type_t type) {
  hal_storage_t *storage = (hal_storage_t *)calloc(1, sizeof(hal_storage_t));
  if (!storage)
    return NULL;

  strncpy(storage->path, path, sizeof(storage->path) - 1);
  storage->type = type;
  storage->initialized = false;

  if (type == HAL_STORAGE_TYPE_FLASH) {
    strncpy(storage->mount_point, "/flash", sizeof(storage->mount_point) - 1);
  } else if (type == HAL_STORAGE_TYPE_SD_SPI ||
             type == HAL_STORAGE_TYPE_SD_SDIO) {
    strncpy(storage->mount_point, "/sd", sizeof(storage->mount_point) - 1);
  }

  return storage;
}

void hal_storage_close(hal_storage_t *storage) {
  if (!storage)
    return;
  hal_storage_deinit(storage);
  free(storage);
}

int hal_storage_init(hal_storage_t *storage) {
  if (!storage || storage->initialized)
    return -1;

  if (storage->type == HAL_STORAGE_TYPE_FLASH) {
    // Initialize LittleFS
    if (!LittleFS.begin()) {
      // Try formatting if mount fails
      if (!LittleFS.begin()) {
        return -1;
      }
    }
    storage->initialized = true;
    return 0;
  }

  // SD card not implemented for Arduino build
  return -1;
}

int hal_storage_deinit(hal_storage_t *storage) {
  if (!storage || !storage->initialized)
    return -1;

  if (storage->type == HAL_STORAGE_TYPE_FLASH) {
    LittleFS.end();
  }

  storage->initialized = false;
  return 0;
}

int hal_storage_read(hal_storage_t *storage, uint32_t offset, void *buffer,
                     size_t size) {
  if (!storage || !storage->initialized || !buffer)
    return -1;
  // Raw flash read not directly supported via LittleFS
  // This would require ESP-IDF APIs
  return -1;
}

int hal_storage_write(hal_storage_t *storage, uint32_t offset,
                      const void *buffer, size_t size) {
  if (!storage || !storage->initialized || !buffer)
    return -1;
  return -1;
}

int hal_storage_erase(hal_storage_t *storage, uint32_t offset, size_t size) {
  if (!storage || !storage->initialized)
    return -1;
  return -1;
}

int hal_storage_sync(hal_storage_t *storage) {
  if (!storage || !storage->initialized)
    return -1;
  return 0;
}

int hal_storage_get_info(hal_storage_t *storage, hal_storage_info_t *info) {
  if (!storage || !info)
    return -1;

  if (storage->type == HAL_STORAGE_TYPE_FLASH && storage->initialized) {
    // Get LittleFS info
    FSInfo fs_info;
    if (LittleFS.info(fs_info)) {
      info->total_bytes = fs_info.totalBytes;
      info->used_bytes = fs_info.usedBytes;
      info->free_bytes = fs_info.totalBytes - fs_info.usedBytes;
      info->block_size = fs_info.blockSize;
      info->page_size = fs_info.pageSize;
      return 0;
    }
  }
  return -1;
}

int hal_storage_set_callback(hal_storage_t *storage, hal_storage_callback_t cb,
                             void *arg) {
  (void)storage;
  (void)cb;
  (void)arg;
  return 0;
}

const char *hal_storage_get_path(const hal_storage_t *storage) {
  return storage ? storage->path : NULL;
}

hal_storage_type_t hal_storage_get_type(const hal_storage_t *storage) {
  return storage ? storage->type : HAL_STORAGE_TYPE_FLASH;
}

int hal_storage_suspend(hal_storage_t *storage) {
  if (!storage)
    return -1;
  storage->initialized = false;
  return 0;
}

int hal_storage_resume(hal_storage_t *storage) {
  if (!storage)
    return -1;
  storage->initialized = true;
  return 0;
}

int hal_storage_file_open(hal_storage_t *storage, const char *path,
                          const char *mode, void **handle) {
  if (!storage || !storage->initialized)
    return -1;

  // LittleFS.open uses "r", "w", "a", "r+", "w+", "a+" modes
  File file = LittleFS.open(path, mode);
  if (!file)
    return -1;
  *handle = (void *)new File(file);
  return 0;
}

int hal_storage_file_read(void *handle, void *buffer, size_t size) {
  File *file = (File *)handle;
  if (!file)
    return -1;
  return file->read((uint8_t *)buffer, size);
}

int hal_storage_file_write(void *handle, const void *buffer, size_t size) {
  File *file = (File *)handle;
  if (!file)
    return -1;
  return file->write((const uint8_t *)buffer, size);
}

int hal_storage_file_seek(void *handle, long offset, int whence) {
  File *file = (File *)handle;
  if (!file)
    return -1;
  SeekMode seek_mode;
  switch (whence) {
  case SEEK_SET:
    seek_mode = SeekSet;
    break;
  case SEEK_CUR:
    seek_mode = SeekCur;
    break;
  case SEEK_END:
    seek_mode = SeekEnd;
    break;
  default:
    return -1;
  }
  return file->seek(offset, seek_mode);
}

int hal_storage_file_tell(void *handle) {
  File *file = (File *)handle;
  if (!file)
    return -1;
  return file->position();
}

int hal_storage_file_close(void *handle) {
  File *file = (File *)handle;
  if (!file)
    return -1;
  file->close();
  delete file;
  return 0;
}

} // extern "C"
