#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARDMOD_MAGIC 0x4D4F4441  // "AMOD"
#define ARDMOD_VERSION 1
#define ARDMOD_MAX_SYMBOLS 64
#define ARDMOD_MAX_DEPS 16
#define ARDMOD_NAME_MAX 32

typedef enum {
    ARDMOD_TYPE_DRIVER = 0,
    ARDMOD_TYPE_APP,
    ARDMOD_TYPE_LIBRARY,
    ARDMOD_TYPE_CODEC,
    ARDMOD_TYPE_PROTOCOL
} ardmod_type_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t size;
    uint32_t crc32;
    char name[ARDMOD_NAME_MAX];
    char version_str[16];
    ardmod_type_t type;
    uint32_t load_addr;
    uint32_t entry_point;
    uint32_t bss_size;
    uint32_t data_size;
    uint32_t symbol_count;
    uint32_t dep_count;
    uint32_t manifest_offset;
    uint32_t code_offset;
} ardmod_header_t;

typedef struct {
    char name[ARDMOD_NAME_MAX];
    uint32_t address;
    uint32_t type;  // 0=function, 1=data, 2=section
} ardmod_symbol_t;

typedef struct {
    char name[ARDMOD_NAME_MAX];
    char version[16];
    bool optional;
} ardmod_dep_t;

typedef struct ardmod_handle ardmod_handle_t;

ardmod_handle_t* ardmod_load(const uint8_t* data, size_t size);
void ardmod_unload(ardmod_handle_t* handle);

int ardmod_register_symbols(ardmod_handle_t* handle);
int ardmod_resolve_deps(ardmod_handle_t* handle);

void* ardmod_get_symbol(ardmod_handle_t* handle, const char* name);
int ardmod_get_symbols(ardmod_handle_t* handle, ardmod_symbol_t* symbols, size_t max_symbols, size_t* count);

int ardmod_verify_crc(const uint8_t* data, size_t size);
int ardmod_calculate_crc(const uint8_t* data, size_t size, uint32_t* crc);

const char* ardmod_get_name(ardmod_handle_t* handle);
const char* ardmod_get_version(ardmod_handle_t* handle);
uint32_t ardmod_get_entry_point(ardmod_handle_t* handle);

int ardmod_create(const char* name, ardmod_type_t type, const uint8_t* code, size_t code_size,
                  const ardmod_symbol_t* symbols, size_t symbol_count,
                  const ardmod_dep_t* deps, size_t dep_count,
                  uint8_t** output, size_t* output_size);

#ifdef __cplusplus
}
#endif