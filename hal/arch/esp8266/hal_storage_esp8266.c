#include "hal_storage.h"
#include "hal_power.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "hal_storage";

#define LITTLEFS_PARTITION "storage"
#define SD_CARD_MOUNT_POINT "/sd"
#define FLASH_MOUNT_POINT "/flash"

typedef struct hal_storage {
    char path[32];
    hal_storage_type_t type;
    bool initialized;
    esp_partition_t* partition;
    sdmmc_card_t* card;
    char mount_point[32];
} hal_storage_t;

hal_storage_t* hal_storage_open(const char* path, hal_storage_type_t type) {
    hal_storage_t* storage = calloc(1, sizeof(hal_storage_t));
    if (!storage) return NULL;
    
    strncpy(storage->path, path, sizeof(storage->path) - 1);
    storage->type = type;
    storage->initialized = false;
    
    if (type == HAL_STORAGE_TYPE_FLASH) {
        strncpy(storage->mount_point, "/flash", sizeof(storage->mount_point) - 1);
    } else if (type == HAL_STORAGE_TYPE_SD_SPI || type == HAL_STORAGE_TYPE_SD_SDIO) {
        strncpy(storage->mount_point, "/sd", sizeof(storage->mount_point) - 1);
    }
    
    return storage;
}

void hal_storage_close(hal_storage_t* storage) {
    if (!storage) return;
    hal_storage_deinit(storage);
    free(storage);
}

int hal_storage_init(hal_storage_t* storage) {
    if (!storage || storage->initialized) return -1;
    
    esp_err_t err = ESP_OK;
    
    if (storage->type == HAL_STORAGE_TYPE_FLASH) {
        const esp_partition_t* partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "storage");
        if (!partition) {
            partition = esp_partition_find_first(
                ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
        }
        
        if (!partition) {
            ESP_LOGE(TAG, "Storage partition not found");
            return -1;
        }
        
        storage->partition = (esp_partition_t*)partition;
        
        esp_vfs_littlefs_conf_t conf = {
            .base_path = storage->mount_point,
            .partition_label = partition->label,
            .format_if_mount_failed = true,
            .dont_mount = false,
        };
        
        err = esp_vfs_littlefs_register(&conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount LittleFS: %s", esp_err_to_name(err));
            return -1;
        }
        
    } else if (storage->type == HAL_STORAGE_TYPE_SD_SPI) {
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = 10000;
        
        spi_bus_config_t bus_cfg = {
            .mosi_io_num = GPIO_NUM_13,
            .miso_io_num = GPIO_NUM_12,
            .sclk_io_num = GPIO_NUM_14,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4096,
        };
        
        err = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) return -1;
        
        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = GPIO_NUM_5;
        slot_config.host_id = SPI2_HOST;
        
        err = esp_vfs_fat_sdspi_mount(storage->mount_point, &host, &slot_config, 
                                      NULL, &storage->card);
        if (err != ESP_OK) {
            spi_bus_free(SPI2_HOST);
            return -1;
        }
        
    } else if (storage->type == HAL_STORAGE_TYPE_SD_SDIO) {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.width = 4;
        slot_config.clk = GPIO_NUM_14;
        slot_config.cmd = GPIO_NUM_15;
        slot_config.d0 = GPIO_NUM_2;
        slot_config.d1 = GPIO_NUM_4;
        slot_config.d2 = GPIO_NUM_12;
        slot_config.d3 = GPIO_NUM_13;
        
        err = esp_vfs_fat_sdmmc_mount(storage->mount_point, &host, &slot_config, 
                                      NULL, &storage->card);
        if (err != ESP_OK) return -1;
    }
    
    if (err == ESP_OK) {
        storage->initialized = true;
        return 0;
    }
    return -1;
}

int hal_storage_deinit(hal_storage_t* storage) {
    if (!storage || !storage->initialized) return -1;
    
    if (storage->type == HAL_STORAGE_TYPE_FLASH) {
        esp_vfs_littlefs_unregister(NULL);
    } else {
        esp_vfs_fat_sdcard_unmount(storage->mount_point, storage->card);
        if (storage->type == HAL_STORAGE_TYPE_SD_SPI) {
            spi_bus_free(SPI2_HOST);
        }
    }
    
    storage->initialized = false;
    return 0;
}

int hal_storage_read(hal_storage_t* storage, uint32_t offset, void* buffer, size_t size) {
    if (!storage || !storage->initialized || !buffer) return -1;
    return -1;
}

int hal_storage_write(hal_storage_t* storage, uint32_t offset, const void* buffer, size_t size) {
    if (!storage || !storage->initialized || !buffer) return -1;
    return -1;
}

int hal_storage_erase(hal_storage_t* storage, uint32_t offset, size_t size) {
    if (!storage || !storage->initialized) return -1;
    return -1;
}

int hal_storage_sync(hal_storage_t* storage) {
    if (!storage || !storage->initialized) return -1;
    return 0;
}

int hal_storage_get_info(hal_storage_t* storage, hal_storage_info_t* info) {
    if (!storage || !info) return -1;
    
    if (storage->type == HAL_STORAGE_TYPE_FLASH && storage->partition) {
        info->total_bytes = storage->partition->size;
        info->used_bytes = 0;
        info->free_bytes = storage->partition->size;
        info->block_size = 4096;
        info->page_size = 256;
    } else if (storage->card) {
        info->total_bytes = storage->card->csd.capacity * storage->card->csd.sector_size;
        info->used_bytes = 0;
        info->free_bytes = info->total_bytes;
        info->block_size = storage->card->csd.sector_size;
        info->page_size = 512;
    }
    return 0;
}

int hal_storage_set_callback(hal_storage_t* storage, hal_storage_callback_t cb, void* arg) {
    (void)storage; (void)cb; (void)arg;
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

int hal_storage_file_open(hal_storage_t* storage, const char* path, const char* mode, void** handle) {
    FILE* f = fopen(path, mode);
    if (!f) return -1;
    *handle = f;
    return 0;
}

int hal_storage_file_read(void* handle, void* buffer, size_t size) {
    return fread(buffer, 1, size, (FILE*)handle);
}

int hal_storage_file_write(void* handle, const void* buffer, size_t size) {
    return fwrite(buffer, 1, size, (FILE*)handle);
}

int hal_storage_file_seek(void* handle, long offset, int whence) {
    return fseek((FILE*)handle, offset, whence);
}

int hal_storage_file_tell(void* handle) {
    return ftell((FILE*)handle);
}

int hal_storage_file_close(void* handle) {
    return fclose((FILE*)handle);
}