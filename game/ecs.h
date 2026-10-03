#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal fixed-memory ECS - the "ECS scene graph (~2KB base)" PLAN.md's
 * Phase 5 calls for. Entities are an index + generation handle (stale
 * handles after destroy are detected, not silently misused); components
 * are plain structs stored in a fixed-size array per type, indexed by
 * entity slot - no per-component malloc churn, matching every other
 * fixed-table module in this codebase (apps/input.c, apps/ui.c, ...).
 *
 * Deliberately not here yet: a renderer, sprites/tilemaps, scripting, the
 * asset pipeline - those are the rest of Phase 5 and depend on decisions
 * (software rasterizer? Lua sandboxing?) this module doesn't need to make.
 */

#define ECS_MAX_ENTITIES 128
#define ECS_MAX_COMPONENT_TYPES 16

typedef struct {
    uint16_t index;
    uint16_t generation;
} ecs_entity_t;

#define ECS_NULL_ENTITY ((ecs_entity_t){0xFFFF, 0})

typedef struct ecs_world ecs_world_t;

ecs_world_t* ecs_world_create(void);
void ecs_world_destroy(ecs_world_t* world);

ecs_entity_t ecs_entity_create(ecs_world_t* world);
void ecs_entity_destroy(ecs_world_t* world, ecs_entity_t e);
bool ecs_entity_is_alive(const ecs_world_t* world, ecs_entity_t e);
uint32_t ecs_entity_count(const ecs_world_t* world);

/* `component_id` is caller-assigned (e.g. an enum a game defines), not
 * allocated by this module - same convention as syscall_num_t/event
 * topics elsewhere in this codebase. Must be < ECS_MAX_COMPONENT_TYPES. */
int ecs_component_register(ecs_world_t* world, uint32_t component_id, size_t size);

/* Returns a pointer to the (uninitialized) component storage for `e`, or
 * NULL if `e` is stale/dead or `component_id` was never registered. The
 * pointer is stable until ecs_component_remove()/ecs_entity_destroy() for
 * that entity+component, or ecs_world_destroy(). */
void* ecs_component_add(ecs_world_t* world, ecs_entity_t e, uint32_t component_id);
void* ecs_component_get(ecs_world_t* world, ecs_entity_t e, uint32_t component_id);
bool ecs_component_has(const ecs_world_t* world, ecs_entity_t e, uint32_t component_id);
void ecs_component_remove(ecs_world_t* world, ecs_entity_t e, uint32_t component_id);

typedef void (*ecs_system_fn_t)(ecs_world_t* world, ecs_entity_t e, void* user);

/* Calls `fn` once for every alive entity that has ALL of `component_ids`
 * (a basic system query). Order follows entity slot index. */
void ecs_each(ecs_world_t* world, const uint32_t* component_ids, int count,
              ecs_system_fn_t fn, void* user);

#ifdef __cplusplus
}
#endif
