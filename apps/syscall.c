#include "syscall.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_SYSCALLS 256

static syscall_entry_t g_syscalls[SYS_MAX] = {0};
static bool g_syscall_initialized = false;

int syscall_init(void) {
    if (g_syscall_initialized) return 0;
    
    memset(g_syscalls, 0, sizeof(g_syscalls));
    g_syscall_initialized = true;
    return 0;
}

void syscall_deinit(void) {
    memset(g_syscalls, 0, sizeof(g_syscalls));
    g_syscall_initialized = false;
}

int syscall_register(const syscall_entry_t* entry) {
    if (!entry || entry->num >= SYS_MAX || entry->num < 0) return -1;
    if (!entry->handler) return -1;
    
    g_syscalls[entry->num] = *entry;
    return 0;
}

int syscall_unregister(syscall_num_t num) {
    if (num >= SYS_MAX || num < 0) return -1;
    
    g_syscalls[num].handler = NULL;
    g_syscalls[num].num = 0;
    return 0;
}

int syscall_invoke(syscall_num_t num, uint32_t* args, uint32_t* ret) {
    if (!g_syscall_initialized) return -1;
    if (num >= SYS_MAX || num < 0) return -1;
    
    syscall_entry_t* entry = &g_syscalls[num];
    if (!entry->handler) return -1;
    
    return entry->handler(args, ret);
}

capability_set_t* capability_set_create(void) {
    capability_set_t* set = calloc(1, sizeof(capability_set_t));
    return set;
}

void capability_set_destroy(capability_set_t* set) {
    free(set);
}

void capability_set_add(capability_set_t* set, capability_t cap) {
    if (!set) return;
    set->caps[cap / 32] |= (1u << (cap % 32));
}

void capability_set_remove(capability_set_t* set, capability_t cap) {
    if (!set) return;
    set->caps[cap / 32] &= ~(1u << (cap % 32));
}

bool capability_set_has(const capability_set_t* set, capability_t cap) {
    if (!set) return false;
    return (set->caps[cap / 32] & (1u << (cap % 32))) != 0;
}

void capability_set_clear(capability_set_t* set) {
    if (!set) return;
    memset(set->caps, 0, sizeof(set->caps));
}

void capability_set_copy(capability_set_t* dst, const capability_set_t* src) {
    if (!dst || !src) return;
    memcpy(dst->caps, src->caps, sizeof(dst->caps));
}

struct app_context {
    uint32_t app_id;
    capability_set_t capabilities;
    void* private_data;
};

app_context_t* app_context_create(uint32_t app_id, const capability_set_t* caps) {
    app_context_t* ctx = calloc(1, sizeof(app_context_t));
    if (!ctx) return NULL;
    
    ctx->app_id = app_id;
    if (caps) {
        capability_set_copy(&ctx->capabilities, caps);
    }
    return ctx;
}

void app_context_destroy(app_context_t* ctx) {
    if (!ctx) return;
    free(ctx);
}

int app_context_check_cap(app_context_t* ctx, capability_t cap) {
    if (!ctx) return -1;
    return capability_set_has(&ctx->capabilities, cap) ? 0 : -1;
}