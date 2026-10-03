#include "unity.h"
#include "ecs.h"
#include <string.h>

/* game/ecs.h - the "ECS scene graph (~2KB base)" from PLAN.md's Phase 5,
 * built fresh this session (game/ had no code at all before this). */

typedef struct { float x, y; } position_t;
typedef struct { float dx, dy; } velocity_t;

#define COMPONENT_POSITION 0
#define COMPONENT_VELOCITY 1

static ecs_world_t* g_world;

void setUp(void) {
    g_world = ecs_world_create();
}

void tearDown(void) {
    ecs_world_destroy(g_world);
}

void test_create_entity_is_alive_and_counted(void) {
    ecs_entity_t e = ecs_entity_create(g_world);
    TEST_ASSERT_TRUE(ecs_entity_is_alive(g_world, e));
    TEST_ASSERT_EQUAL(1, ecs_entity_count(g_world));
}

void test_destroy_entity_is_no_longer_alive(void) {
    ecs_entity_t e = ecs_entity_create(g_world);
    ecs_entity_destroy(g_world, e);
    TEST_ASSERT_FALSE(ecs_entity_is_alive(g_world, e));
    TEST_ASSERT_EQUAL(0, ecs_entity_count(g_world));
}

void test_stale_handle_is_detected_after_slot_reuse(void) {
    ecs_entity_t first = ecs_entity_create(g_world);
    ecs_entity_destroy(g_world, first);

    ecs_entity_t second = ecs_entity_create(g_world);  // likely reuses first's slot

    TEST_ASSERT_FALSE(ecs_entity_is_alive(g_world, first));
    TEST_ASSERT_TRUE(ecs_entity_is_alive(g_world, second));
    TEST_ASSERT_EQUAL(first.index, second.index);
    TEST_ASSERT_NOT_EQUAL(first.generation, second.generation);
}

void test_component_register_add_get(void) {
    TEST_ASSERT_EQUAL(0, ecs_component_register(g_world, COMPONENT_POSITION, sizeof(position_t)));

    ecs_entity_t e = ecs_entity_create(g_world);
    TEST_ASSERT_FALSE(ecs_component_has(g_world, e, COMPONENT_POSITION));

    position_t* pos = (position_t*)ecs_component_add(g_world, e, COMPONENT_POSITION);
    TEST_ASSERT_NOT_NULL(pos);
    pos->x = 3.0f;
    pos->y = 4.0f;

    TEST_ASSERT_TRUE(ecs_component_has(g_world, e, COMPONENT_POSITION));
    position_t* got = (position_t*)ecs_component_get(g_world, e, COMPONENT_POSITION);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, got->x);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, got->y);
}

void test_component_remove_clears_presence_but_keeps_entity_alive(void) {
    ecs_component_register(g_world, COMPONENT_POSITION, sizeof(position_t));
    ecs_entity_t e = ecs_entity_create(g_world);
    ecs_component_add(g_world, e, COMPONENT_POSITION);

    ecs_component_remove(g_world, e, COMPONENT_POSITION);

    TEST_ASSERT_FALSE(ecs_component_has(g_world, e, COMPONENT_POSITION));
    TEST_ASSERT_TRUE(ecs_entity_is_alive(g_world, e));
}

void test_destroying_entity_clears_its_components(void) {
    ecs_component_register(g_world, COMPONENT_POSITION, sizeof(position_t));
    ecs_entity_t e = ecs_entity_create(g_world);
    ecs_component_add(g_world, e, COMPONENT_POSITION);

    ecs_entity_destroy(g_world, e);

    // Same (dead) handle - has() must reject it rather than read stale data.
    TEST_ASSERT_FALSE(ecs_component_has(g_world, e, COMPONENT_POSITION));
}

void test_get_on_entity_without_the_component_is_null(void) {
    ecs_component_register(g_world, COMPONENT_POSITION, sizeof(position_t));
    ecs_entity_t e = ecs_entity_create(g_world);
    TEST_ASSERT_NULL(ecs_component_get(g_world, e, COMPONENT_POSITION));
}

void test_unregistered_component_id_is_rejected(void) {
    ecs_entity_t e = ecs_entity_create(g_world);
    TEST_ASSERT_NULL(ecs_component_add(g_world, e, COMPONENT_VELOCITY));
}

static int g_each_calls;
static float g_sum_x;

static void sum_position_x(ecs_world_t* world, ecs_entity_t e, void* user) {
    (void)user;
    position_t* pos = (position_t*)ecs_component_get(world, e, COMPONENT_POSITION);
    g_sum_x += pos->x;
    g_each_calls++;
}

void test_each_only_visits_entities_with_all_requested_components(void) {
    ecs_component_register(g_world, COMPONENT_POSITION, sizeof(position_t));
    ecs_component_register(g_world, COMPONENT_VELOCITY, sizeof(velocity_t));

    ecs_entity_t with_both = ecs_entity_create(g_world);
    ((position_t*)ecs_component_add(g_world, with_both, COMPONENT_POSITION))->x = 10.0f;
    ecs_component_add(g_world, with_both, COMPONENT_VELOCITY);

    ecs_entity_t position_only = ecs_entity_create(g_world);
    ((position_t*)ecs_component_add(g_world, position_only, COMPONENT_POSITION))->x = 100.0f;

    ecs_entity_t neither = ecs_entity_create(g_world);
    (void)neither;

    g_each_calls = 0;
    g_sum_x = 0.0f;
    uint32_t ids[] = {COMPONENT_POSITION, COMPONENT_VELOCITY};
    ecs_each(g_world, ids, 2, sum_position_x, NULL);

    TEST_ASSERT_EQUAL(1, g_each_calls);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, g_sum_x);  // not 110 - position_only must be excluded
}

void test_each_skips_destroyed_entities(void) {
    ecs_component_register(g_world, COMPONENT_POSITION, sizeof(position_t));
    ecs_entity_t e = ecs_entity_create(g_world);
    ecs_component_add(g_world, e, COMPONENT_POSITION);
    ecs_entity_destroy(g_world, e);

    g_each_calls = 0;
    uint32_t ids[] = {COMPONENT_POSITION};
    ecs_each(g_world, ids, 1, sum_position_x, NULL);
    TEST_ASSERT_EQUAL(0, g_each_calls);
}

void test_world_exhausts_entity_slots_gracefully(void) {
    ecs_entity_t last = ECS_NULL_ENTITY;
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        last = ecs_entity_create(g_world);
        TEST_ASSERT_TRUE(ecs_entity_is_alive(g_world, last));
    }
    ecs_entity_t overflow = ecs_entity_create(g_world);
    TEST_ASSERT_FALSE(ecs_entity_is_alive(g_world, overflow));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_entity_is_alive_and_counted);
    RUN_TEST(test_destroy_entity_is_no_longer_alive);
    RUN_TEST(test_stale_handle_is_detected_after_slot_reuse);
    RUN_TEST(test_component_register_add_get);
    RUN_TEST(test_component_remove_clears_presence_but_keeps_entity_alive);
    RUN_TEST(test_destroying_entity_clears_its_components);
    RUN_TEST(test_get_on_entity_without_the_component_is_null);
    RUN_TEST(test_unregistered_component_id_is_rejected);
    RUN_TEST(test_each_only_visits_entities_with_all_requested_components);
    RUN_TEST(test_each_skips_destroyed_entities);
    RUN_TEST(test_world_exhausts_entity_slots_gracefully);
    return UNITY_END();
}
