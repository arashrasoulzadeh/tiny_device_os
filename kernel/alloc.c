#include "alloc.h"
#include <string.h>
#include <stdlib.h>

#define TLSF_SIGNATURE 0x544C5346
#define TLSF_BLOCK_FREE 0x01
#define TLSF_BLOCK_USED 0x02

typedef struct block_header {
    uint32_t signature;
    uint32_t size;
    uint8_t flags;
    uint8_t padding[3];
    struct block_header* prev_free;
    struct block_header* next_free;
    struct block_header* prev_phys;
    struct block_header* next_phys;
} block_header_t;

struct tlsf_pool {
    uint32_t signature;
    size_t total_size;
    size_t used_size;
    size_t min_free;
    block_header_t* free_list[32];
    block_header_t* block_list;
    uint8_t* memory;
};

#define BLOCK_OVERHEAD sizeof(block_header_t)
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define MIN_BLOCK_SIZE (BLOCK_OVERHEAD + TLSF_BLOCK_ALIGN)

static inline uint32_t tlsf_fls(uint32_t x) {
    uint32_t r = 0;
    if (x >= 0x10000) { x >>= 16; r += 16; }
    if (x >= 0x100)   { x >>= 8;  r += 8;  }
    if (x >= 0x10)    { x >>= 4;  r += 4;  }
    if (x >= 0x4)     { x >>= 2;  r += 2;  }
    if (x >= 0x2)     { r += 1; }
    return r;
}

static inline int tlsf_ffs(uint32_t x) {
    if (!x) return -1;
    int r = 0;
    if (!(x & 0xFFFF)) { x >>= 16; r += 16; }
    if (!(x & 0xFF))   { x >>= 8;  r += 8;  }
    if (!(x & 0xF))    { x >>= 4;  r += 4;  }
    if (!(x & 0x3))    { x >>= 2;  r += 2;  }
    if (!(x & 0x1))    { r += 1; }
    return r;
}

static inline int tlsf_get_fl_index(size_t size) {
    return tlsf_fls(size);
}

static inline int tlsf_get_sl_index(size_t size, int fl) {
    return (int)((size ^ (1u << fl)) >> (fl > 5 ? fl - 5 : 0));
}

static inline int tlsf_get_index(size_t size) {
    int fl = tlsf_get_fl_index(size);
    return (fl << 3) + tlsf_get_sl_index(size, fl);
}

static block_header_t* block_from_ptr(void* ptr) {
    return (block_header_t*)((uint8_t*)ptr - BLOCK_OVERHEAD);
}

static void* ptr_from_block(block_header_t* block) {
    return (uint8_t*)block + BLOCK_OVERHEAD;
}

static void block_insert_free(tlsf_pool_t* pool, block_header_t* block) {
    int idx = tlsf_get_index(block->size);
    block->flags |= TLSF_BLOCK_FREE;
    block->next_free = pool->free_list[idx];
    block->prev_free = NULL;
    if (pool->free_list[idx]) {
        pool->free_list[idx]->prev_free = block;
    }
    pool->free_list[idx] = block;
}

static void block_remove_free(tlsf_pool_t* pool, block_header_t* block) {
    int idx = tlsf_get_index(block->size);
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        pool->free_list[idx] = block->next_free;
    }
    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    }
    block->flags &= ~TLSF_BLOCK_FREE;
}

static block_header_t* block_split(tlsf_pool_t* pool, block_header_t* block, size_t size) {
    size_t remaining = block->size - size - BLOCK_OVERHEAD;
    if (remaining < MIN_BLOCK_SIZE) {
        return NULL;
    }
    
    block_header_t* new_block = (block_header_t*)((uint8_t*)block + size + BLOCK_OVERHEAD);
    new_block->signature = TLSF_SIGNATURE;
    new_block->size = remaining;
    new_block->flags = 0;
    new_block->prev_phys = block;
    new_block->next_phys = block->next_phys;
    
    if (block->next_phys) {
        block->next_phys->prev_phys = new_block;
    }
    block->next_phys = new_block;
    block->size = size;
    
    block_insert_free(pool, new_block);
    
    return new_block;
}

static block_header_t* block_merge(tlsf_pool_t* pool, block_header_t* block) {
    if (block->prev_phys && (block->prev_phys->flags & TLSF_BLOCK_FREE)) {
        block_header_t* prev = block->prev_phys;
        block_remove_free(pool, prev);
        prev->size += BLOCK_OVERHEAD + block->size;
        prev->next_phys = block->next_phys;
        if (block->next_phys) {
            block->next_phys->prev_phys = prev;
        }
        block = prev;
    }
    
    if (block->next_phys && (block->next_phys->flags & TLSF_BLOCK_FREE)) {
        block_header_t* next = block->next_phys;
        block_remove_free(pool, next);
        block->size += BLOCK_OVERHEAD + next->size;
        block->next_phys = next->next_phys;
        if (next->next_phys) {
            next->next_phys->prev_phys = block;
        }
    }
    
    return block;
}

tlsf_pool_t* tlsf_create(void* mem, size_t bytes) {
    if (!mem || bytes < sizeof(tlsf_pool_t) + MIN_BLOCK_SIZE) {
        return NULL;
    }
    
    tlsf_pool_t* pool = (tlsf_pool_t*)mem;
    memset(pool, 0, sizeof(tlsf_pool_t));
    
    pool->signature = TLSF_SIGNATURE;
    pool->total_size = bytes - sizeof(tlsf_pool_t);
    pool->used_size = 0;
    pool->min_free = pool->total_size;
    pool->memory = (uint8_t*)mem + sizeof(tlsf_pool_t);
    
    block_header_t* block = (block_header_t*)pool->memory;
    block->signature = TLSF_SIGNATURE;
    block->size = pool->total_size - BLOCK_OVERHEAD;
    block->flags = 0;
    block->prev_phys = NULL;
    block->next_phys = NULL;
    
    pool->block_list = block;
    block_insert_free(pool, block);
    
    return pool;
}

void tlsf_destroy(tlsf_pool_t* pool) {
    (void)pool;
}

void* tlsf_malloc(tlsf_pool_t* pool, size_t size) {
    if (!pool || size == 0) return NULL;
    
    size = ALIGN_UP(size, TLSF_BLOCK_ALIGN);
    size_t total_size = size + BLOCK_OVERHEAD;
    
    int idx = tlsf_get_index(total_size);
    
    for (int fl = idx >> 3; fl < 32; fl++) {
        int sl_start = (fl == (idx >> 3)) ? (idx & 7) : 0;
        uint32_t fl_map = 0;
        for (int sl = sl_start; sl < 8; sl++) {
            if (pool->free_list[(fl << 3) + sl]) {
                fl_map |= (1u << sl);
            }
        }
        
        if (fl_map) {
            int sl = tlsf_ffs(fl_map);
            int list_idx = (fl << 3) + sl;
            
            block_header_t* block = pool->free_list[list_idx];
            block_remove_free(pool, block);
            
            if (block->size >= total_size + MIN_BLOCK_SIZE) {
                block_split(pool, block, size);
            }
            
            block->flags |= TLSF_BLOCK_USED;
            pool->used_size += block->size + BLOCK_OVERHEAD;
            size_t free = tlsf_pool_free_size(pool);
            if (free < pool->min_free) pool->min_free = free;
            
            return ptr_from_block(block);
        }
    }
    
    return NULL;
}

void* tlsf_memalign(tlsf_pool_t* pool, size_t align, size_t size) {
    if (!pool || size == 0) return NULL;
    if ((align & (align - 1)) != 0) return NULL;
    
    size = ALIGN_UP(size, TLSF_BLOCK_ALIGN);
    align = ALIGN_UP(align, TLSF_BLOCK_ALIGN);
    
    void* ptr = tlsf_malloc(pool, size + align);
    if (!ptr) return NULL;
    
    uintptr_t aligned = ALIGN_UP((uintptr_t)ptr, align);
    if (aligned != (uintptr_t)ptr) {
        size_t offset = aligned - (uintptr_t)ptr;
        if (offset >= MIN_BLOCK_SIZE) {
            block_header_t* block = block_from_ptr(ptr);
            block_header_t* new_block = (block_header_t*)((uint8_t*)block + offset);
            
            block_remove_free(pool, block);
            
            new_block->signature = TLSF_SIGNATURE;
            new_block->size = block->size - offset - BLOCK_OVERHEAD;
            new_block->flags = 0;
            new_block->prev_phys = block;
            new_block->next_phys = block->next_phys;
            
            if (block->next_phys) {
                block->next_phys->prev_phys = new_block;
            }
            block->next_phys = new_block;
            block->size = offset;
            
            block_insert_free(pool, new_block);
            
            block->flags |= TLSF_BLOCK_USED;
            pool->used_size += block->size + BLOCK_OVERHEAD;
            
            return ptr_from_block(block);
        }
    }
    
    return (void*)aligned;
}

void* tlsf_realloc(tlsf_pool_t* pool, void* ptr, size_t size) {
    if (!ptr) return tlsf_malloc(pool, size);
    if (size == 0) { tlsf_free(pool, ptr); return NULL; }
    
    block_header_t* block = block_from_ptr(ptr);
    if (block->signature != TLSF_SIGNATURE) return NULL;
    
    size = ALIGN_UP(size, TLSF_BLOCK_ALIGN);
    size_t total_size = size + BLOCK_OVERHEAD;
    
    if (block->size >= total_size) {
        if (block->size >= total_size + MIN_BLOCK_SIZE) {
            block_split(pool, block, size);
        }
        return ptr;
    }
    
    block_header_t* next = block->next_phys;
    if (next && (next->flags & TLSF_BLOCK_FREE) &&
        (block->size + BLOCK_OVERHEAD + next->size) >= total_size) {
        
        block_remove_free(pool, next);
        block->size += BLOCK_OVERHEAD + next->size;
        block->next_phys = next->next_phys;
        if (next->next_phys) {
            next->next_phys->prev_phys = block;
        }
        
        if (block->size >= total_size + MIN_BLOCK_SIZE) {
            block_split(pool, block, size);
        }
        return ptr;
    }
    
    void* new_ptr = tlsf_malloc(pool, size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        tlsf_free(pool, ptr);
    }
    return new_ptr;
}

void tlsf_free(tlsf_pool_t* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    block_header_t* block = block_from_ptr(ptr);
    if (block->signature != TLSF_SIGNATURE) return;
    if (!(block->flags & TLSF_BLOCK_USED)) return;
    
    block->flags &= ~TLSF_BLOCK_USED;
    pool->used_size -= block->size + BLOCK_OVERHEAD;
    
    block = block_merge(pool, block);
    block_insert_free(pool, block);
}

size_t tlsf_pool_free_size(tlsf_pool_t* pool) {
    if (!pool) return 0;
    size_t free = 0;
    for (int i = 0; i < 32; i++) {
        block_header_t* block = pool->free_list[i];
        while (block) {
            free += block->size + BLOCK_OVERHEAD;
            block = block->next_free;
        }
    }
    return free;
}

size_t tlsf_pool_used_size(tlsf_pool_t* pool) {
    return pool ? pool->used_size : 0;
}

size_t tlsf_pool_total_size(tlsf_pool_t* pool) {
    return pool ? pool->total_size : 0;
}

bool tlsf_pool_check(tlsf_pool_t* pool) {
    if (!pool || pool->signature != TLSF_SIGNATURE) return false;
    
    block_header_t* block = pool->block_list;
    while (block) {
        if (block->signature != TLSF_SIGNATURE) return false;
        block = block->next_phys;
    }
    return true;
}

static tlsf_pool_t* g_system_pool = NULL;
static uint8_t g_heap_memory[32 * 1024];

void* os_malloc(size_t size) {
    if (!g_system_pool) {
        g_system_pool = tlsf_create(g_heap_memory, sizeof(g_heap_memory));
    }
    return tlsf_malloc(g_system_pool, size);
}

void* os_calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = os_malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void* os_realloc(void* ptr, size_t size) {
    if (!g_system_pool) {
        g_system_pool = tlsf_create(g_heap_memory, sizeof(g_heap_memory));
    }
    return tlsf_realloc(g_system_pool, ptr, size);
}

void os_free(void* ptr) {
    if (g_system_pool && ptr) {
        tlsf_free(g_system_pool, ptr);
    }
}

size_t os_get_free_heap(void) {
    if (!g_system_pool) {
        g_system_pool = tlsf_create(g_heap_memory, sizeof(g_heap_memory));
    }
    return tlsf_pool_free_size(g_system_pool);
}

size_t os_get_min_free_heap(void) {
    if (!g_system_pool) {
        g_system_pool = tlsf_create(g_heap_memory, sizeof(g_heap_memory));
    }
    return g_system_pool->min_free;
}