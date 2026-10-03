#include "ota.h"
#include "ed25519.h"
#include "hal_storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define OTA_METADATA_OFFSET 0
#define OTA_METADATA_SIZE 4096

struct ota_handle {
    hal_storage_t* storage;
    ota_partition_t partition;
    uint32_t offset;
    uint32_t expected_size;
    uint32_t written;
    uint32_t crc32;
    ota_progress_cb_t progress_cb;
    void* progress_arg;
};

/* This used to be a hand-transcribed 256-entry table. It had a corrupted
 * entry (index 51 read 0xBF002116; the real standard CRC-32 value is
 * 0xBFD06116) and, separately, 4 extra bogus entries past the real 256 -
 * clang caught the extras as -Wexcess-initializers, but the corrupted
 * value compiled silently and would have made every CRC this module
 * computed wrong for any byte sequence that hit that table bucket. Since
 * OTA's whole job is verifying firmware integrity, a silently-wrong CRC
 * table is exactly the kind of bug most worth not hand-transcribing 256
 * magic numbers for. Generated once from the standard polynomial instead -
 * same technique drivers/module.c's own (separate, correct) CRC32 already
 * uses. */
static uint32_t crc32_table[256];
static bool crc32_table_ready = false;

static void crc32_init_table(void) {
    if (crc32_table_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_ready = true;
}

uint32_t ota_crc32_update(uint32_t crc, const void* data, size_t len) {
    crc32_init_table();
    const uint8_t* p = (const uint8_t*)data;
    crc = ~crc;
    while (len--) {
        crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

static int ota_read_metadata(hal_storage_t* storage, ota_metadata_t* meta) {
    if (!storage || !meta) return -1;
    
    int ret = hal_storage_read(storage, OTA_METADATA_OFFSET, meta, sizeof(ota_metadata_t));
    if (ret != 0) return -1;
    
    if (meta->magic != OTA_MAGIC) return -1;
    return 0;
}

static int ota_write_metadata(hal_storage_t* storage, const ota_metadata_t* meta) {
    if (!storage || !meta) return -1;
    
    int ret = hal_storage_write(storage, OTA_METADATA_OFFSET, meta, sizeof(ota_metadata_t));
    if (ret != 0) return -1;
    
    return hal_storage_sync(storage);
}

ota_handle_t* ota_begin(hal_storage_t* storage, ota_partition_t partition,
                        uint32_t expected_size, ota_progress_cb_t cb, void* arg) {
    if (!storage || partition >= OTA_PARTITION_MAX) return NULL;
    
    ota_handle_t* handle = calloc(1, sizeof(ota_handle_t));
    if (!handle) return NULL;
    
    handle->storage = storage;
    handle->partition = partition;
    handle->expected_size = expected_size;
    handle->progress_cb = cb;
    handle->progress_arg = arg;
    handle->crc32 = 0;
    handle->written = 0;
    
    // Calculate partition offset (simplified - would need partition table)
    handle->offset = 0x10000 + partition * 0x100000;
    
    return handle;
}

int ota_write(ota_handle_t* handle, const void* data, size_t size) {
    if (!handle || !data || size == 0) return -1;
    
    if (handle->written + size > handle->expected_size) {
        size = handle->expected_size - handle->written;
    }
    
    int ret = hal_storage_write(handle->storage, handle->offset + handle->written, data, size);
    if (ret != 0) return -1;
    
    handle->crc32 = ota_crc32_update(handle->crc32, data, size);
    handle->written += size;
    
    if (handle->progress_cb) {
        handle->progress_cb(handle->written, handle->expected_size, handle->progress_arg);
    }
    
    return (int)size;
}

int ota_end(ota_handle_t* handle, bool verify_signature) {
    if (!handle) return -1;
    
    // Sync storage
    hal_storage_sync(handle->storage);
    
    // Update metadata
    ota_metadata_t meta;
    if (ota_read_metadata(handle->storage, &meta) != 0) {
        // Initialize new metadata
        memset(&meta, 0, sizeof(meta));
        meta.magic = OTA_MAGIC;
        meta.version = OTA_VERSION;
        meta.active_partition = OTA_PARTITION_OTA_0;
        meta.next_partition = handle->partition;
        meta.sequence = 1;
    } else {
        meta.sequence++;
        meta.next_partition = handle->partition;
        meta.next_size = handle->written;
        meta.next_crc32 = handle->crc32;
        meta.next_state = verify_signature ? OTA_STATE_PENDING_VERIFY : OTA_STATE_NEW;
        meta.timestamp = 0; // Would use RTC
    }
    
    if (ota_write_metadata(handle->storage, &meta) != 0) {
        free(handle);
        return -1;
    }
    
    free(handle);
    return 0;
}

void ota_abort(ota_handle_t* handle) {
    if (handle) free(handle);
}

int ota_set_boot_partition(ota_partition_t partition) {
    if (partition >= OTA_PARTITION_MAX) return -1;
    
    // This would typically write to a bootloader config area
    // Simplified implementation
    return 0;
}

ota_partition_t ota_get_boot_partition(void) {
    ota_metadata_t meta;
    hal_storage_t* storage = NULL; // Would need actual storage handle
    if (ota_read_metadata(storage, &meta) == 0) {
        return meta.active_partition;
    }
    return OTA_PARTITION_OTA_0;
}

ota_partition_t ota_get_running_partition(void) {
    // This would be determined at boot time by the bootloader
    return OTA_PARTITION_OTA_0;
}

int ota_verify_signature(const uint8_t* firmware, uint32_t size,
                         const uint8_t* signature, size_t sig_size,
                         const uint8_t* pubkey, size_t key_size) {
    if (!firmware || size == 0 || !signature || sig_size != 64 || !pubkey || key_size != 32) {
        return -1;
    }
    
    // Verify Ed25519 signature
    int ret = ed25519_verify(signature, firmware, size, pubkey);
    return ret == 0 ? 0 : -1;
}

int ota_get_metadata(ota_metadata_t* meta) {
    if (!meta) return -1;
    
    hal_storage_t* storage = NULL; // Would need actual storage handle
    return ota_read_metadata(storage, meta);
}

int ota_mark_valid(ota_partition_t partition) {
    ota_metadata_t meta;
    hal_storage_t* storage = NULL;
    if (ota_read_metadata(storage, &meta) != 0) return -1;
    
    if (partition == meta.next_partition) {
        meta.active_partition = partition;
        meta.active_size = meta.next_size;
        meta.active_crc32 = meta.next_crc32;
        meta.active_state = OTA_STATE_VALID;
        meta.next_partition = OTA_PARTITION_MAX;
        meta.next_size = 0;
        meta.next_crc32 = 0;
        meta.next_state = OTA_STATE_NEW;
        meta.sequence++;
        
        return ota_write_metadata(storage, &meta);
    }
    return -1;
}

int ota_mark_invalid(ota_partition_t partition) {
    ota_metadata_t meta;
    hal_storage_t* storage = NULL;
    if (ota_read_metadata(storage, &meta) != 0) return -1;
    
    if (partition == meta.next_partition) {
        meta.next_state = OTA_STATE_INVALID;
        return ota_write_metadata(storage, &meta);
    }
    if (partition == meta.active_partition) {
        meta.active_state = OTA_STATE_INVALID;
        return ota_write_metadata(storage, &meta);
    }
    return -1;
}

int ota_rollback(void) {
    ota_metadata_t meta;
    hal_storage_t* storage = NULL;
    if (ota_read_metadata(storage, &meta) != 0) return -1;
    
    // Swap active and next (if next was valid)
    if (meta.next_state == OTA_STATE_VALID) {
        ota_partition_t old_active = meta.active_partition;
        meta.active_partition = meta.next_partition;
        meta.next_partition = old_active;
        
        uint32_t old_active_size = meta.active_size;
        meta.active_size = meta.next_size;
        meta.next_size = old_active_size;
        
        uint32_t old_active_crc = meta.active_crc32;
        meta.active_crc32 = meta.next_crc32;
        meta.next_crc32 = old_active_crc;
        
        ota_state_t old_active_state = meta.active_state;
        meta.active_state = meta.next_state;
        meta.next_state = old_active_state;
        
        meta.sequence++;
        return ota_write_metadata(storage, &meta);
    }
    return -1;
}