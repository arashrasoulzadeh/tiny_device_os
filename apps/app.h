#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "syscall.h"
#include "scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_NAME_MAX 32
#define APP_MAX 16

typedef enum {
    APP_STATE_NOT_INSTALLED = 0,
    APP_STATE_INSTALLED,
    APP_STATE_STOPPED,
    APP_STATE_RUNNING,
    APP_STATE_SUSPENDED,
    APP_STATE_ERROR
} app_state_t;

typedef enum {
    APP_TYPE_SYSTEM = 0,
    APP_TYPE_USER,
    APP_TYPE_GAME,
    APP_TYPE_TOOL
} app_type_t;

typedef struct app_manifest {
    char name[APP_NAME_MAX];
    char version[16];
    app_type_t type;
    uint32_t min_os_version;
    uintptr_t entry_point;
    uint32_t stack_size;
    uint32_t heap_size;
    capability_t capabilities[16];
    uint32_t capability_count;
    char author[64];
    char description[256];
} app_manifest_t;

typedef struct app_limits {
    uint32_t max_cpu_time_ms;
    uint32_t max_memory_bytes;
    uint32_t max_storage_bytes;
    uint32_t max_file_handles;
    uint32_t max_timers;
} app_limits_t;

typedef struct app {
    char name[APP_NAME_MAX];
    app_state_t state;
    app_type_t type;
    uint32_t app_id;
    app_manifest_t manifest;
    app_limits_t limits;
    app_context_t* context;
    void* module_handle;
    task_tcb_t* task;
    uint32_t start_time;
    uint32_t cpu_time_used;
    uint32_t memory_used;
    struct app* next;
} app_t;

int app_init(void);
void app_deinit(void);

int app_install(const uint8_t* module_data, size_t module_size, const char* name);
int app_install_manifest(const app_manifest_t* manifest, const char* name);
int app_uninstall(const char* name);

int app_start(const char* name);
int app_stop(const char* name);
int app_suspend(const char* name);
int app_resume(const char* name);

app_t* app_find(const char* name);
app_t* app_find_by_id(uint32_t app_id);

int app_get_info(const char* name, app_manifest_t* manifest, app_state_t* state);
int app_list(app_t** apps, size_t max_apps, size_t* count);

int app_set_limits(const char* name, const app_limits_t* limits);
int app_get_limits(const char* name, app_limits_t* limits);

int app_get_resource_usage(const char* name, uint32_t* cpu_time, uint32_t* memory);

// System key API
int os_app_switch_next(void);
int os_app_quit_current(void);
int os_sys_show_menu(void);

#ifdef __cplusplus
}
#endif