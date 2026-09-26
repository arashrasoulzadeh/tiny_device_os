#include "app.h"
#include "module.h"
#include "syscall.h"
#include "scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static app_t* g_apps = NULL;
static uint32_t g_next_app_id = 1;
static int g_app_lock = 0;

static void app_lock(void) {
    while (__sync_lock_test_and_set(&g_app_lock, 1)) {}
}

static void app_unlock(void) {
    __sync_lock_release(&g_app_lock);
}

static void app_task_entry(void* arg) {
    printf("app_task_entry called\n");
    fflush(stdout);
    app_t* app = (app_t*)arg;
    if (!app || !app->context) {
        printf("app_task_entry: app or context is NULL\n");
        fflush(stdout);
        return;
    }
    
    typedef void (*app_entry_t)(void);
    app_entry_t entry = (app_entry_t)app->manifest.entry_point;
    
    if (entry) {
        printf("Calling app entry point: %p\n", (void*)entry);
        fflush(stdout);
        entry();
    } else {
        printf("Entry point is NULL\n");
        fflush(stdout);
    }
    
    printf("App entry returned\n");
    fflush(stdout);
    
    app_stop(app->name);
    task_delete(app->task);
}

int app_init(void) {
    g_apps = NULL;
    g_next_app_id = 1;
    g_app_lock = 0;
    return 0;
}

void app_deinit(void) {
    app_lock();
    while (g_apps) {
        app_t* app = g_apps;
        g_apps = app->next;
        if (app->context) app_context_destroy(app->context);
        if (app->module_handle) ardmod_unload(app->module_handle);
        free(app);
    }
    app_unlock();
}

static int parse_manifest(ardmod_handle_t* handle, app_manifest_t* manifest) {
    if (!handle || !manifest) return -1;
    
    void* sym = ardmod_get_symbol(handle, "app_manifest");
    if (sym) {
        memcpy(manifest, sym, sizeof(app_manifest_t));
        return 0;
    }
    
    memset(manifest, 0, sizeof(app_manifest_t));
    strncpy(manifest->name, ardmod_get_name(handle), APP_NAME_MAX - 1);
    strncpy(manifest->version, ardmod_get_version(handle), 15);
    manifest->entry_point = ardmod_get_entry_point(handle);
    manifest->type = APP_TYPE_USER;
    return 0;
}

int app_install(const uint8_t* module_data, size_t module_size, const char* name) {
    if (!module_data || !name || module_size == 0) return -1;
    
    ardmod_handle_t* handle = ardmod_load(module_data, module_size);
    if (!handle) return -1;
    
    if (ardmod_resolve_deps(handle) != 0) {
        ardmod_unload(handle);
        return -1;
    }
    
    if (ardmod_register_symbols(handle) != 0) {
        ardmod_unload(handle);
        return -1;
    }
    
    app_manifest_t manifest;
    if (parse_manifest(handle, &manifest) != 0) {
        ardmod_unload(handle);
        return -1;
    }
    
    app_lock();
    for (app_t* a = g_apps; a; a = a->next) {
        if (strcmp(a->name, name) == 0) {
            app_unlock();
            ardmod_unload(handle);
            return -1;
        }
    }
    app_unlock();
    
    capability_set_t caps;
    capability_set_clear(&caps);
    for (uint32_t i = 0; i < manifest.capability_count; i++) {
        capability_set_add(&caps, manifest.capabilities[i]);
    }
    
    app_context_t* ctx = app_context_create(g_next_app_id, &caps);
    if (!ctx) {
        ardmod_unload(handle);
        return -1;
    }
    
    app_t* app = calloc(1, sizeof(app_t));
    if (!app) {
        app_context_destroy(ctx);
        ardmod_unload(handle);
        return -1;
    }
    
    strncpy(app->name, name, APP_NAME_MAX - 1);
    app->state = APP_STATE_INSTALLED;
    app->type = manifest.type;
    app->app_id = g_next_app_id++;
    app->manifest = manifest;
    app->context = ctx;
    app->module_handle = handle;
    
    app->limits.max_cpu_time_ms = 0;
    app->limits.max_memory_bytes = manifest.heap_size > 0 ? manifest.heap_size : 65536;
    app->limits.max_storage_bytes = 0;
    app->limits.max_file_handles = 32;
    app->limits.max_timers = 16;
    
    app_lock();
    app->next = g_apps;
    g_apps = app;
    app_unlock();
    
    return 0;
}

int app_install_manifest(const app_manifest_t* manifest, const char* name) {
    if (!manifest || !name) return -1;
    
    app_lock();
    for (app_t* a = g_apps; a; a = a->next) {
        if (strcmp(a->name, name) == 0) {
            app_unlock();
            return -1;
        }
    }
    app_unlock();
    
    capability_set_t caps;
    capability_set_clear(&caps);
    for (uint32_t i = 0; i < manifest->capability_count; i++) {
        capability_set_add(&caps, manifest->capabilities[i]);
    }
    
    app_context_t* ctx = app_context_create(g_next_app_id, &caps);
    if (!ctx) {
        return -1;
    }
    
    app_t* app = calloc(1, sizeof(app_t));
    if (!app) {
        app_context_destroy(ctx);
        return -1;
    }
    
    strncpy(app->name, name, APP_NAME_MAX - 1);
    app->state = APP_STATE_INSTALLED;
    app->type = manifest->type;
    app->app_id = g_next_app_id++;
    app->manifest = *manifest;
    app->context = ctx;
    app->module_handle = NULL;
    
    app->limits.max_cpu_time_ms = 0;
    app->limits.max_memory_bytes = manifest->heap_size > 0 ? manifest->heap_size : 65536;
    app->limits.max_storage_bytes = 0;
    app->limits.max_file_handles = 32;
    app->limits.max_timers = 16;
    
    app_lock();
    app->next = g_apps;
    g_apps = app;
    app_unlock();
    
    return 0;
}

int app_uninstall(const char* name) {
    if (!name) return -1;
    
    app_lock();
    
    app_t** prev = &g_apps;
    for (app_t* a = g_apps; a; a = a->next) {
        if (strcmp(a->name, name) == 0) {
            if (a->state == APP_STATE_RUNNING) {
                app_unlock();
                return -1;
            }
            
            *prev = a->next;
            
            if (a->context) app_context_destroy(a->context);
            if (a->module_handle) ardmod_unload(a->module_handle);
            free(a);
            
            app_unlock();
            return 0;
        }
        prev = &a->next;
    }
    
    app_unlock();
    return -1;
}

int app_start(const char* name) {
    if (!name) return -1;
    
    app_lock();
    app_t* app = NULL;
    for (app_t* a = g_apps; a; a = a->next) {
        if (strcmp(a->name, name) == 0) {
            app = a;
            break;
        }
    }
    app_unlock();
    
    if (!app) return -1;
    if (app->state != APP_STATE_INSTALLED && app->state != APP_STATE_STOPPED) {
        return -1;
    }
    
    // For all apps, create a task to run the entry point
    task_tcb_t* task;
    int ret = task_create(app->name, app_task_entry, app, 
                         TASK_PRIO_NORMAL, app->manifest.stack_size > 0 ? 
                         app->manifest.stack_size : 4096, &task);
    if (ret != 0) return -1;
    
    app->task = task;
    app->state = APP_STATE_RUNNING;
    app->start_time = scheduler_get_tick_count();
    app->cpu_time_used = 0;
    
    return 0;
}

int app_stop(const char* name) {
    if (!name) return -1;
    
    app_lock();
    app_t* app = NULL;
    for (app_t* a = g_apps; a; a = a->next) {
        if (strcmp(a->name, name) == 0) {
            app = a;
            break;
        }
    }
    app_unlock();
    
    if (!app) return -1;
    if (app->state != APP_STATE_RUNNING) return -1;
    
    if (app->task) {
        task_delete(app->task);
        app->task = NULL;
    }
    
    app->state = APP_STATE_STOPPED;
    app->cpu_time_used += scheduler_get_tick_count() - app->start_time;
    
    return 0;
}

int app_suspend(const char* name) {
    if (!name) return -1;
    
    app_t* app = app_find(name);
    if (!app) return -1;
    if (app->state != APP_STATE_RUNNING) return -1;
    
    if (app->task) {
        task_suspend(app->task);
    }
    app->state = APP_STATE_SUSPENDED;
    return 0;
}

int app_resume(const char* name) {
    if (!name) return -1;
    
    app_t* app = app_find(name);
    if (!app) return -1;
    if (app->state != APP_STATE_SUSPENDED) return -1;
    
    if (app->task) {
        task_resume(app->task);
    }
    app->state = APP_STATE_RUNNING;
    return 0;
}

app_t* app_find(const char* name) {
    if (!name) return NULL;
    
    app_lock();
    for (app_t* a = g_apps; a; a = a->next) {
        if (strcmp(a->name, name) == 0) {
            app_unlock();
            return a;
        }
    }
    app_unlock();
    return NULL;
}

app_t* app_find_by_id(uint32_t app_id) {
    app_lock();
    for (app_t* a = g_apps; a; a = a->next) {
        if (a->app_id == app_id) {
            app_unlock();
            return a;
        }
    }
    app_unlock();
    return NULL;
}

int app_get_info(const char* name, app_manifest_t* manifest, app_state_t* state) {
    app_t* app = app_find(name);
    if (!app) return -1;
    
    if (manifest) *manifest = app->manifest;
    if (state) *state = app->state;
    return 0;
}

int app_list(app_t** apps, size_t max_apps, size_t* count) {
    if (!apps || !count) return -1;
    
    app_lock();
    size_t n = 0;
    for (app_t* a = g_apps; a && n < max_apps; a = a->next) {
        apps[n++] = a;
    }
    app_unlock();
    
    *count = n;
    return 0;
}

int app_set_limits(const char* name, const app_limits_t* limits) {
    if (!name || !limits) return -1;
    
    app_t* app = app_find(name);
    if (!app) return -1;
    
    app->limits = *limits;
    return 0;
}

int app_get_limits(const char* name, app_limits_t* limits) {
    if (!name || !limits) return -1;
    
    app_t* app = app_find(name);
    if (!app) return -1;
    
    *limits = app->limits;
    return 0;
}

int app_get_resource_usage(const char* name, uint32_t* cpu_time, uint32_t* memory) {
    app_t* app = app_find(name);
    if (!app) return -1;
    
    if (cpu_time) {
        if (app->state == APP_STATE_RUNNING) {
            *cpu_time = app->cpu_time_used + (scheduler_get_tick_count() - app->start_time);
        } else {
            *cpu_time = app->cpu_time_used;
        }
    }
    if (memory) {
        *memory = app->memory_used;
    }
    return 0;
}

// System key API implementations
int os_app_switch_next(void) {
    app_t* current = NULL;
    app_t* next = NULL;
    bool found_current = false;
    
    app_lock();
    for (app_t* a = g_apps; a; a = a->next) {
        if (a->state == APP_STATE_RUNNING) {
            current = a;
            found_current = true;
        } else if (found_current && a->state == APP_STATE_INSTALLED) {
            next = a;
            break;
        }
    }
    
    // If no next app found, wrap to first installed app
    if (!next) {
        for (app_t* a = g_apps; a; a = a->next) {
            if (a->state == APP_STATE_INSTALLED) {
                next = a;
                break;
            }
        }
    }
    
    if (current) {
        app_stop(current->name);
    }
    if (next) {
        app_start(next->name);
    }
    app_unlock();
    return next ? 0 : -1;
}

int os_app_quit_current(void) {
    app_t* current = NULL;
    
    app_lock();
    for (app_t* a = g_apps; a; a = a->next) {
        if (a->state == APP_STATE_RUNNING) {
            current = a;
            break;
        }
    }
    app_unlock();
    
    if (current) {
        return app_stop(current->name);
    }
    return -1;
}

int os_sys_show_menu(void) {
    // TODO: Implement system menu display
    printf("System menu not yet implemented\n");
    fflush(stdout);
    return 0;
}