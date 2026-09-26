#include "module.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define CRC32_POLY 0xEDB88320

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void crc32_init_table(void) {
    if (crc32_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (CRC32_POLY ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

uint32_t crc32_update(uint32_t crc, const void* data, size_t len) {
    crc32_init_table();
    const uint8_t* p = (const uint8_t*)data;
    crc = ~crc;
    while (len--) {
        crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

struct ardmod_handle {
    ardmod_header_t header;
    uint8_t* data;
    size_t size;
    ardmod_symbol_t* symbols;
    ardmod_dep_t* deps;
    bool loaded;
    void* reloc_base;
};

ardmod_handle_t* ardmod_load(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(ardmod_header_t)) return NULL;
    
    ardmod_handle_t* handle = calloc(1, sizeof(ardmod_handle_t));
    if (!handle) return NULL;
    
    // Copy header
    memcpy(&handle->header, data, sizeof(ardmod_header_t));
    
    // Verify magic
    if (handle->header.magic != ARDMOD_MAGIC) {
        free(handle);
        return NULL;
    }
    
    // Verify version
    if (handle->header.version != ARDMOD_VERSION) {
        free(handle);
        return NULL;
    }
    
    // Verify CRC
    if (!ardmod_verify_crc(data, size)) {
        free(handle);
        return NULL;
    }
    
    // Copy data
    handle->data = malloc(size);
    if (!handle->data) {
        free(handle);
        return NULL;
    }
    memcpy(handle->data, data, size);
    handle->size = size;
    
    // Parse symbols
    if (handle->header.symbol_count > 0 && handle->header.manifest_offset > 0) {
        handle->symbols = malloc(handle->header.symbol_count * sizeof(ardmod_symbol_t));
        if (handle->symbols) {
            memcpy(handle->symbols, data + handle->header.manifest_offset,
                   handle->header.symbol_count * sizeof(ardmod_symbol_t));
        }
    }
    
    // Parse dependencies
    if (handle->header.dep_count > 0) {
        uint32_t dep_offset = handle->header.manifest_offset + 
                              handle->header.symbol_count * sizeof(ardmod_symbol_t);
        handle->deps = malloc(handle->header.dep_count * sizeof(ardmod_dep_t));
        if (handle->deps) {
            memcpy(handle->deps, data + dep_offset,
                   handle->header.dep_count * sizeof(ardmod_dep_t));
        }
    }
    
    handle->loaded = true;
    return handle;
}

void ardmod_unload(ardmod_handle_t* handle) {
    if (!handle) return;
    
    free(handle->symbols);
    free(handle->deps);
    free(handle->data);
    free(handle);
}

int ardmod_verify_crc(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(ardmod_header_t)) return 0;
    
    ardmod_header_t* header = (ardmod_header_t*)data;
    uint32_t stored_crc = header->crc32;
    header->crc32 = 0;
    
    uint32_t calc_crc = crc32_update(0, data, size);
    
    header->crc32 = stored_crc;
    
    return calc_crc == stored_crc;
}

int ardmod_calculate_crc(const uint8_t* data, size_t size, uint32_t* crc) {
    if (!data || !crc) return -1;
    *crc = crc32_update(0, data, size);
    return 0;
}

int ardmod_register_symbols(ardmod_handle_t* handle) {
    if (!handle || !handle->loaded) return -1;
    // In a real implementation, this would register symbols in a global table
    return 0;
}

int ardmod_resolve_deps(ardmod_handle_t* handle) {
    if (!handle || !handle->loaded) return -1;
    // In a real implementation, this would load dependencies
    return 0;
}

void* ardmod_get_symbol(ardmod_handle_t* handle, const char* name) {
    if (!handle || !handle->loaded || !name) return NULL;
    
    for (uint32_t i = 0; i < handle->header.symbol_count; i++) {
        if (strcmp(handle->symbols[i].name, name) == 0) {
            // Return address relative to load address
            return (void*)(handle->header.load_addr + handle->symbols[i].address);
        }
    }
    return NULL;
}

int ardmod_get_symbols(ardmod_handle_t* handle, ardmod_symbol_t* symbols, 
                       size_t max_symbols, size_t* count) {
    if (!handle || !handle->loaded || !count) return -1;
    
    size_t copy_count = handle->header.symbol_count < max_symbols ? 
                        handle->header.symbol_count : max_symbols;
    if (symbols) {
        memcpy(symbols, handle->symbols, copy_count * sizeof(ardmod_symbol_t));
    }
    *count = handle->header.symbol_count;
    return 0;
}

int ardmod_create(const char* name, ardmod_type_t type, const uint8_t* code, size_t code_size,
                  const ardmod_symbol_t* symbols, size_t symbol_count,
                  const ardmod_dep_t* deps, size_t dep_count,
                  uint8_t** output, size_t* output_size) {
    if (!name || !code || !output || !output_size) return -1;
    if (symbol_count > ARDMOD_MAX_SYMBOLS) return -1;
    if (dep_count > ARDMOD_MAX_DEPS) return -1;
    
    size_t manifest_size = symbol_count * sizeof(ardmod_symbol_t) + 
                          dep_count * sizeof(ardmod_dep_t);
    size_t total_size = sizeof(ardmod_header_t) + manifest_size + code_size;
    
    uint8_t* data = calloc(1, total_size);
    if (!data) return -1;
    
    ardmod_header_t* header = (ardmod_header_t*)data;
    header->magic = ARDMOD_MAGIC;
    header->version = ARDMOD_VERSION;
    header->flags = 0;
    header->size = total_size;
    header->crc32 = 0;
    strncpy(header->name, name, ARDMOD_NAME_MAX - 1);
    strncpy(header->version_str, "1.0.0", 15);
    header->type = type;
    header->load_addr = 0;
    header->entry_point = 0;
    header->bss_size = 0;
    header->data_size = 0;
    header->symbol_count = symbol_count;
    header->dep_count = dep_count;
    header->manifest_offset = sizeof(ardmod_header_t);
    header->code_offset = sizeof(ardmod_header_t) + manifest_size;
    
    // Copy symbols
    if (symbol_count > 0 && symbols) {
        memcpy(data + header->manifest_offset, symbols, 
               symbol_count * sizeof(ardmod_symbol_t));
    }
    
    // Copy dependencies
    if (dep_count > 0 && deps) {
        uint32_t dep_offset = header->manifest_offset + 
                              symbol_count * sizeof(ardmod_symbol_t);
        memcpy(data + dep_offset, deps, dep_count * sizeof(ardmod_dep_t));
    }
    
    // Copy code
    memcpy(data + header->code_offset, code, code_size);
    
    // Calculate CRC
    header->crc32 = 0;
    uint32_t crc = crc32_update(0, data, total_size);
    header->crc32 = crc;
    
    *output = data;
    *output_size = total_size;
    return 0;
}

const char* ardmod_get_name(ardmod_handle_t* handle) {
    if (!handle) return NULL;
    return handle->header.name;
}

const char* ardmod_get_version(ardmod_handle_t* handle) {
    if (!handle) return NULL;
    return handle->header.version_str;
}

uint32_t ardmod_get_entry_point(ardmod_handle_t* handle) {
    if (!handle) return 0;
    return handle->header.entry_point;
}