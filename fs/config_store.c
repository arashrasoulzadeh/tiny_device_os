#include "config_store.h"
#include "vfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CONFIG_MAGIC 0x43464753
#define CONFIG_VERSION 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t count;
    uint32_t data_offset;
    uint32_t data_size;
} config_header_t;

typedef struct {
    uint32_t key_hash;
    uint16_t key_len;
    uint16_t type;
    uint32_t offset;
    uint32_t size;
} config_entry_t;

#define CONFIG_TYPE_STRING 1
#define CONFIG_TYPE_INT    2
#define CONFIG_TYPE_BOOL   3
#define CONFIG_TYPE_BLOB   4

struct config_store {
    vfs_file_t* file;
    config_header_t header;
    config_entry_t* entries;
    uint8_t* data;
    bool dirty;
};

static uint32_t hash_key(const char* key) {
    uint32_t hash = 5381;
    while (*key) {
        hash = ((hash << 5) + hash) + *key++;
    }
    return hash;
}

static int config_write_header(config_store_t* store);
static int config_reserve_data(config_store_t* store, size_t size);
static config_entry_t* config_find_entry(config_store_t* store, const char* key);
static int config_store_flush(config_store_t* store);

config_store_t* config_store_open(const char* path) {
    if (!path) return NULL;
    
    config_store_t* store = calloc(1, sizeof(config_store_t));
    if (!store) return NULL;
    
    vfs_mode_t mode = VFS_MODE_READ | VFS_MODE_WRITE | VFS_MODE_CREATE;
    int ret = vfs_open(path, mode, &store->file);
    if (ret != 0) {
        free(store);
        return NULL;
    }
    
    return store;
}

void config_store_close(config_store_t* store) {
    if (!store) return;
    config_store_flush(store);
    if (store->file) vfs_close(store->file);
    free(store->entries);
    free(store->data);
    free(store);
}

int config_store_init(config_store_t* store) {
    if (!store || !store->file) return -1;
    
    // Read header
    vfs_seek(store->file, 0, VFS_SEEK_SET);
    ssize_t read = vfs_read(store->file, &store->header, sizeof(config_header_t));
    if (read != sizeof(config_header_t)) {
        // Initialize new store
        store->header.magic = CONFIG_MAGIC;
        store->header.version = CONFIG_VERSION;
        store->header.count = 0;
        store->header.data_offset = sizeof(config_header_t) + 1024 * sizeof(config_entry_t);
        store->header.data_size = 0;
        return 0;
    }
    
    if (store->header.magic != CONFIG_MAGIC) return -1;
    
    // Read entries
    if (store->header.count > 0) {
        store->entries = calloc(store->header.count, sizeof(config_entry_t));
        if (!store->entries) return -1;
        
        vfs_seek(store->file, sizeof(config_header_t), VFS_SEEK_SET);
        read = vfs_read(store->file, store->entries, 
                       store->header.count * sizeof(config_entry_t));
        if (read != (ssize_t)(store->header.count * sizeof(config_entry_t))) {
            free(store->entries);
            store->entries = NULL;
            return -1;
        }
    }
    
    // Read data
    if (store->header.data_size > 0) {
        store->data = calloc(1, store->header.data_size);
        if (!store->data) return -1;
        
        vfs_seek(store->file, store->header.data_offset, VFS_SEEK_SET);
        read = vfs_read(store->file, store->data, store->header.data_size);
        if (read != (ssize_t)store->header.data_size) {
            free(store->data);
            store->data = NULL;
            return -1;
        }
    }
    
    return 0;
}

int config_store_deinit(config_store_t* store) {
    if (!store) return -1;
    config_store_flush(store);
    free(store->entries);
    store->entries = NULL;
    free(store->data);
    store->data = NULL;
    store->header.count = 0;
    store->header.data_size = 0;
    return 0;
}

static int config_write_header(config_store_t* store) {
    vfs_seek(store->file, 0, VFS_SEEK_SET);
    ssize_t written = vfs_write(store->file, &store->header, sizeof(config_header_t));
    if (written != sizeof(config_header_t)) return -1;
    
    if (store->header.count > 0) {
        vfs_seek(store->file, sizeof(config_header_t), VFS_SEEK_SET);
        written = vfs_write(store->file, store->entries, 
                           store->header.count * sizeof(config_entry_t));
        if (written != (ssize_t)(store->header.count * sizeof(config_entry_t))) return -1;
    }
    
    if (store->header.data_size > 0) {
        vfs_seek(store->file, store->header.data_offset, VFS_SEEK_SET);
        written = vfs_write(store->file, store->data, store->header.data_size);
        if (written != (ssize_t)store->header.data_size) return -1;
    }
    
    return vfs_sync(store->file);
}

int config_store_flush(config_store_t* store) {
    if (!store || !store->dirty) return 0;
    int ret = config_write_header(store);
    if (ret == 0) store->dirty = false;
    return ret;
}

static int config_reserve_data(config_store_t* store, size_t size) {
    if (store->header.data_size + size > 65536) return -1;
    return 0;
}

static config_entry_t* config_find_entry(config_store_t* store, const char* key) {
    uint32_t hash = hash_key(key);
    size_t key_len = strlen(key);
    
    for (uint32_t i = 0; i < store->header.count; i++) {
        if (store->entries[i].key_hash == hash && 
            store->entries[i].key_len == key_len) {
            return &store->entries[i];
        }
    }
    return NULL;
}

int config_set_string(config_store_t* store, const char* key, const char* value) {
    if (!store || !key || !value) return -1;
    
    config_entry_t* entry = config_find_entry(store, key);
    size_t value_len = strlen(value) + 1;
    
    if (entry) {
        if (entry->size >= value_len) {
            memcpy(store->data + entry->offset, value, value_len);
            entry->size = value_len;
            entry->type = CONFIG_TYPE_STRING;
        } else {
            config_delete(store, key);
            return config_set_string(store, key, value);
        }
    } else {
        if (store->header.count >= 1024) return -1;
        if (config_reserve_data(store, value_len) != 0) return -1;
        
        entry = &store->entries[store->header.count++];
        entry->key_hash = hash_key(key);
        entry->key_len = strlen(key);
        entry->type = CONFIG_TYPE_STRING;
        entry->offset = store->header.data_size;
        entry->size = value_len;
        
        store->data = realloc(store->data, store->header.data_size + value_len);
        if (!store->data) return -1;
        
        memcpy(store->data + entry->offset, value, value_len);
        store->header.data_size += value_len;
    }
    
    store->dirty = true;
    return 0;
}

int config_set_int(config_store_t* store, const char* key, int32_t value) {
    if (!store || !key) return -1;
    
    config_entry_t* entry = config_find_entry(store, key);
    
    if (entry) {
        if (entry->size >= sizeof(int32_t)) {
            memcpy(store->data + entry->offset, &value, sizeof(int32_t));
            entry->size = sizeof(int32_t);
            entry->type = CONFIG_TYPE_INT;
        } else {
            config_delete(store, key);
            return config_set_int(store, key, value);
        }
    } else {
        if (store->header.count >= 1024) return -1;
        if (config_reserve_data(store, sizeof(int32_t)) != 0) return -1;
        
        entry = &store->entries[store->header.count++];
        entry->key_hash = hash_key(key);
        entry->key_len = strlen(key);
        entry->type = CONFIG_TYPE_INT;
        entry->offset = store->header.data_size;
        entry->size = sizeof(int32_t);
        
        store->data = realloc(store->data, store->header.data_size + sizeof(int32_t));
        if (!store->data) return -1;
        
        memcpy(store->data + entry->offset, &value, sizeof(int32_t));
        store->header.data_size += sizeof(int32_t);
    }
    
    store->dirty = true;
    return 0;
}

int config_set_bool(config_store_t* store, const char* key, bool value) {
    if (!store || !key) return -1;
    uint8_t v = value ? 1 : 0;
    
    config_entry_t* entry = config_find_entry(store, key);
    
    if (entry) {
        if (entry->size >= 1) {
            store->data[entry->offset] = v;
            entry->size = 1;
            entry->type = CONFIG_TYPE_BOOL;
        } else {
            config_delete(store, key);
            return config_set_bool(store, key, value);
        }
    } else {
        if (store->header.count >= 1024) return -1;
        if (config_reserve_data(store, 1) != 0) return -1;
        
        entry = &store->entries[store->header.count++];
        entry->key_hash = hash_key(key);
        entry->key_len = strlen(key);
        entry->type = CONFIG_TYPE_BOOL;
        entry->offset = store->header.data_size;
        entry->size = 1;
        
        store->data = realloc(store->data, store->header.data_size + 1);
        if (!store->data) return -1;
        
        store->data[entry->offset] = v;
        store->header.data_size += 1;
    }
    
    store->dirty = true;
    return 0;
}

int config_set_blob(config_store_t* store, const char* key, const void* data, size_t size) {
    if (!store || !key || !data || size == 0) return -1;
    
    config_entry_t* entry = config_find_entry(store, key);
    
    if (entry) {
        if (entry->size >= size) {
            memcpy(store->data + entry->offset, data, size);
            entry->size = size;
            entry->type = CONFIG_TYPE_BLOB;
        } else {
            config_delete(store, key);
            return config_set_blob(store, key, data, size);
        }
    } else {
        if (store->header.count >= 1024) return -1;
        if (config_reserve_data(store, size) != 0) return -1;
        
        entry = &store->entries[store->header.count++];
        entry->key_hash = hash_key(key);
        entry->key_len = strlen(key);
        entry->type = CONFIG_TYPE_BLOB;
        entry->offset = store->header.data_size;
        entry->size = size;
        
        store->data = realloc(store->data, store->header.data_size + size);
        if (!store->data) return -1;
        
        memcpy(store->data + entry->offset, data, size);
        store->header.data_size += size;
    }
    
    store->dirty = true;
    return 0;
}

const char* config_get_string(config_store_t* store, const char* key, const char* def) {
    if (!store || !key) return def;
    
    config_entry_t* entry = config_find_entry(store, key);
    if (!entry || entry->type != CONFIG_TYPE_STRING) return def;
    
    return (const char*)(store->data + entry->offset);
}

int32_t config_get_int(config_store_t* store, const char* key, int32_t def) {
    if (!store || !key) return def;
    
    config_entry_t* entry = config_find_entry(store, key);
    if (!entry || entry->type != CONFIG_TYPE_INT) return def;
    
    int32_t val;
    memcpy(&val, store->data + entry->offset, sizeof(int32_t));
    return val;
}

bool config_get_bool(config_store_t* store, const char* key, bool def) {
    if (!store || !key) return def;
    
    config_entry_t* entry = config_find_entry(store, key);
    if (!entry || entry->type != CONFIG_TYPE_BOOL) return def;
    
    return store->data[entry->offset] != 0;
}

int config_get_blob(config_store_t* store, const char* key, void* data, size_t max_size) {
    if (!store || !key || !data || max_size == 0) return -1;
    
    config_entry_t* entry = config_find_entry(store, key);
    if (!entry || entry->type != CONFIG_TYPE_BLOB) return -1;
    
    size_t copy_size = entry->size < max_size ? entry->size : max_size;
    memcpy(data, store->data + entry->offset, copy_size);
    return (int)copy_size;
}

int config_delete(config_store_t* store, const char* key) {
    if (!store || !key) return -1;
    
    config_entry_t* entry = config_find_entry(store, key);
    if (!entry) return -1;
    
    // Shift remaining entries
    for (config_entry_t* e = entry + 1; 
         e < store->entries + store->header.count; e++) {
        *(e - 1) = *e;
    }
    
    store->header.count--;
    store->dirty = true;
    return 0;
}

int config_exists(config_store_t* store, const char* key) {
    if (!store || !key) return 0;
    return config_find_entry(store, key) != NULL ? 1 : 0;
}

int config_list_keys(config_store_t* store, char** keys, size_t max_keys, size_t* count) {
    if (!store || !keys || !count) return -1;
    
    size_t found = 0;
    for (uint32_t i = 0; i < store->header.count && found < max_keys; i++) {
        // We don't store the actual key strings, so we can't return them
        // This would require storing keys in the data area
        keys[found] = NULL;
        found++;
    }
    *count = found;
    return 0;
}