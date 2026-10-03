#include "ecs.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t component_size;
    bool registered;
    uint8_t* data;                          // ECS_MAX_ENTITIES * component_size bytes
    bool present[ECS_MAX_ENTITIES];
} component_store_t;

struct ecs_world {
    uint16_t generation[ECS_MAX_ENTITIES];
    bool alive[ECS_MAX_ENTITIES];
    uint32_t entity_count;
    component_store_t components[ECS_MAX_COMPONENT_TYPES];
};

ecs_world_t* ecs_world_create(void) {
    return calloc(1, sizeof(ecs_world_t));
}

void ecs_world_destroy(ecs_world_t* world) {
    if (!world) return;
    for (int i = 0; i < ECS_MAX_COMPONENT_TYPES; i++) {
        free(world->components[i].data);
    }
    free(world);
}

ecs_entity_t ecs_entity_create(ecs_world_t* world) {
    if (!world) return ECS_NULL_ENTITY;

    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!world->alive[i]) {
            world->alive[i] = true;
            world->entity_count++;
            return (ecs_entity_t){.index = (uint16_t)i, .generation = world->generation[i]};
        }
    }
    return ECS_NULL_ENTITY;
}

void ecs_entity_destroy(ecs_world_t* world, ecs_entity_t e) {
    if (!ecs_entity_is_alive(world, e)) return;

    for (int c = 0; c < ECS_MAX_COMPONENT_TYPES; c++) {
        if (world->components[c].registered) {
            world->components[c].present[e.index] = false;
        }
    }

    world->alive[e.index] = false;
    world->generation[e.index]++;  // invalidates any other outstanding handle to this slot
    world->entity_count--;
}

bool ecs_entity_is_alive(const ecs_world_t* world, ecs_entity_t e) {
    if (!world || e.index >= ECS_MAX_ENTITIES) return false;
    return world->alive[e.index] && world->generation[e.index] == e.generation;
}

uint32_t ecs_entity_count(const ecs_world_t* world) {
    return world ? world->entity_count : 0;
}

int ecs_component_register(ecs_world_t* world, uint32_t component_id, size_t size) {
    if (!world || component_id >= ECS_MAX_COMPONENT_TYPES || size == 0) return -1;

    component_store_t* store = &world->components[component_id];
    if (store->registered) return -1;  // one registration per id, no redefining the size

    store->data = calloc(ECS_MAX_ENTITIES, size);
    if (!store->data) return -1;

    store->component_size = size;
    store->registered = true;
    return 0;
}

void* ecs_component_add(ecs_world_t* world, ecs_entity_t e, uint32_t component_id) {
    if (!ecs_entity_is_alive(world, e) || component_id >= ECS_MAX_COMPONENT_TYPES) return NULL;

    component_store_t* store = &world->components[component_id];
    if (!store->registered) return NULL;

    store->present[e.index] = true;
    return store->data + (size_t)e.index * store->component_size;
}

void* ecs_component_get(ecs_world_t* world, ecs_entity_t e, uint32_t component_id) {
    if (!ecs_entity_is_alive(world, e) || component_id >= ECS_MAX_COMPONENT_TYPES) return NULL;

    component_store_t* store = &world->components[component_id];
    if (!store->registered || !store->present[e.index]) return NULL;

    return store->data + (size_t)e.index * store->component_size;
}

bool ecs_component_has(const ecs_world_t* world, ecs_entity_t e, uint32_t component_id) {
    if (!ecs_entity_is_alive(world, e) || component_id >= ECS_MAX_COMPONENT_TYPES) return false;

    const component_store_t* store = &world->components[component_id];
    return store->registered && store->present[e.index];
}

void ecs_component_remove(ecs_world_t* world, ecs_entity_t e, uint32_t component_id) {
    if (!ecs_entity_is_alive(world, e) || component_id >= ECS_MAX_COMPONENT_TYPES) return;

    component_store_t* store = &world->components[component_id];
    if (store->registered) store->present[e.index] = false;
}

void ecs_each(ecs_world_t* world, const uint32_t* component_ids, int count,
              ecs_system_fn_t fn, void* user) {
    if (!world || !fn || (count > 0 && !component_ids)) return;

    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!world->alive[i]) continue;

        ecs_entity_t e = {.index = (uint16_t)i, .generation = world->generation[i]};
        bool has_all = true;
        for (int c = 0; c < count; c++) {
            if (!ecs_component_has(world, e, component_ids[c])) {
                has_all = false;
                break;
            }
        }
        if (has_all) fn(world, e, user);
    }
}
