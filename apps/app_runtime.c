#include "app_runtime.h"

#include "app.h"

#include <string.h>

typedef struct {
    char name[APP_NAME_MAX];
    uint8_t data[APP_STATE_BYTES];
    size_t size;
    void* live;
    size_t live_size;
    bool used;
} app_runtime_slot_t;

static app_runtime_slot_t g_slots[APP_MAX];

static app_runtime_slot_t* find_slot(const char* name) {
    int i;
    if (!name || !name[0]) {
        return NULL;
    }
    for (i = 0; i < APP_MAX; i++) {
        if (g_slots[i].used && strcmp(g_slots[i].name, name) == 0) {
            return &g_slots[i];
        }
    }
    return NULL;
}

static app_runtime_slot_t* take_slot(const char* name) {
    app_runtime_slot_t* slot = find_slot(name);
    int i;
    if (slot) {
        return slot;
    }
    if (!name || !name[0]) {
        return NULL;
    }
    for (i = 0; i < APP_MAX; i++) {
        if (!g_slots[i].used) {
            memset(&g_slots[i], 0, sizeof(g_slots[i]));
            strncpy(g_slots[i].name, name, APP_NAME_MAX - 1);
            g_slots[i].used = true;
            return &g_slots[i];
        }
    }
    return NULL;
}

int app_set_state(const char* name, const void* data, size_t size) {
    app_runtime_slot_t* slot;
    if (!data || size == 0 || size > APP_STATE_BYTES) {
        return -1;
    }
    slot = take_slot(name);
    if (!slot) {
        return -1;
    }
    memcpy(slot->data, data, size);
    slot->size = size;
    return 0;
}

int app_get_state(const char* name, void* data, size_t size) {
    app_runtime_slot_t* slot = find_slot(name);
    if (!data || !slot || slot->size == 0 || slot->size != size) {
        return -1;
    }
    memcpy(data, slot->data, size);
    return 0;
}

int app_state_bind(const char* name, void* live, size_t size) {
    app_runtime_slot_t* slot;
    if (!live || size == 0 || size > APP_STATE_BYTES) {
        return -1;
    }
    slot = take_slot(name);
    if (!slot) {
        return -1;
    }
    slot->live = live;
    slot->live_size = size;
    return 0;
}

int app_state_capture(const char* name) {
    app_runtime_slot_t* slot = find_slot(name);
    if (!slot || !slot->live || slot->live_size == 0) {
        return 0;
    }
    return app_set_state(name, slot->live, slot->live_size);
}

int app_state_apply(const char* name) {
    app_runtime_slot_t* slot = find_slot(name);
    if (!slot || !slot->live || slot->size == 0 || slot->size != slot->live_size) {
        return -1;
    }
    memcpy(slot->live, slot->data, slot->size);
    return 0;
}
