#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct config_store config_store_t;

config_store_t* config_store_open(const char* path);
void config_store_close(config_store_t* store);

int config_store_init(config_store_t* store);
int config_store_deinit(config_store_t* store);

int config_set_string(config_store_t* store, const char* key, const char* value);
int config_set_int(config_store_t* store, const char* key, int32_t value);
int config_set_bool(config_store_t* store, const char* key, bool value);
int config_set_blob(config_store_t* store, const char* key, const void* data, size_t size);

const char* config_get_string(config_store_t* store, const char* key, const char* def);
int32_t config_get_int(config_store_t* store, const char* key, int32_t def);
bool config_get_bool(config_store_t* store, const char* key, bool def);
int config_get_blob(config_store_t* store, const char* key, void* data, size_t max_size);

int config_delete(config_store_t* store, const char* key);
int config_exists(config_store_t* store, const char* key);

int config_flush(config_store_t* store);
int config_list_keys(config_store_t* store, char** keys, size_t max_keys, size_t* count);

#ifdef __cplusplus
}
#endif