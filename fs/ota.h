#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hal_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_MAGIC 0x4F54414D
#define OTA_VERSION 1
#define OTA_MAX_PARTITIONS 2

typedef enum {
    OTA_PARTITION_OTA_0 = 0,
    OTA_PARTITION_OTA_1,
    OTA_PARTITION_FACTORY,
    OTA_PARTITION_MAX
} ota_partition_t;

typedef enum {
    OTA_STATE_NEW = 0,
    OTA_STATE_PENDING_VERIFY,
    OTA_STATE_VALID,
    OTA_STATE_INVALID,
    OTA_STATE_ABORTED
} ota_state_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t sequence;
    ota_partition_t active_partition;
    ota_partition_t next_partition;
    uint32_t active_crc32;
    uint32_t next_crc32;
    uint32_t active_size;
    uint32_t next_size;
    ota_state_t active_state;
    ota_state_t next_state;
    uint32_t timestamp;
} ota_metadata_t;

typedef struct ota_handle ota_handle_t;

typedef int (*ota_progress_cb_t)(uint32_t progress, uint32_t total, void* arg);

ota_handle_t* ota_begin(hal_storage_t* storage, ota_partition_t partition, 
                        uint32_t expected_size, ota_progress_cb_t cb, void* arg);
int ota_write(ota_handle_t* handle, const void* data, size_t size);
int ota_end(ota_handle_t* handle, bool verify_signature);
void ota_abort(ota_handle_t* handle);

int ota_set_boot_partition(ota_partition_t partition);
ota_partition_t ota_get_boot_partition(void);
ota_partition_t ota_get_running_partition(void);

int ota_verify_signature(const uint8_t* firmware, uint32_t size, 
                         const uint8_t* signature, size_t sig_size,
                         const uint8_t* pubkey, size_t key_size);

int ota_get_metadata(ota_metadata_t* meta);
int ota_mark_valid(ota_partition_t partition);
int ota_mark_invalid(ota_partition_t partition);
int ota_rollback(void);

#ifdef __cplusplus
}
#endif