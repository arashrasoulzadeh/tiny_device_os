#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TLSF_BLOCK_ALIGN 4
#define TLSF_MAX_POOL_SIZE (256 * 1024)

typedef struct tlsf_pool tlsf_pool_t;

tlsf_pool_t* tlsf_create(void* mem, size_t bytes);
void tlsf_destroy(tlsf_pool_t* pool);

void* tlsf_malloc(tlsf_pool_t* pool, size_t size);
void* tlsf_memalign(tlsf_pool_t* pool, size_t align, size_t size);
void* tlsf_realloc(tlsf_pool_t* pool, void* ptr, size_t size);
void tlsf_free(tlsf_pool_t* pool, void* ptr);

size_t tlsf_pool_free_size(tlsf_pool_t* pool);
size_t tlsf_pool_used_size(tlsf_pool_t* pool);
size_t tlsf_pool_total_size(tlsf_pool_t* pool);
bool tlsf_pool_check(tlsf_pool_t* pool);

void* os_malloc(size_t size);
void* os_calloc(size_t nmemb, size_t size);
void* os_realloc(void* ptr, size_t size);
void os_free(void* ptr);

size_t os_get_free_heap(void);
size_t os_get_min_free_heap(void);

#ifdef __cplusplus
}
#endif