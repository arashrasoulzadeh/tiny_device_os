#include "app_kit.h"
#include "power.h"
#include "scheduler.h"
#include "unity.h"
#include <string.h>

static void dummy_entry(void) {}

static void dummy_key(void *app, void *user) {
  (void)app;
  (void)user;
}

void setUp(void) {
  scheduler_init();
  power_init();
}

void tearDown(void) {}

void test_app_kit_make_manifest_fills_fields(void) {
  app_desc_t desc = {
      .name = "demo",
      .version = "1.2.3",
      .author = "Tester",
      .description = "Demo app",
      .type = APP_TYPE_TOOL,
      .fps = 30,
      .stack_size = APP_STACK_MEDIUM,
      .heap_size = APP_HEAP_SMALL,
      .on_init = NULL,
      .on_frame = NULL,
      .on_cleanup = NULL,
  };

  app_manifest_t *m = app_kit_make_manifest(&desc, dummy_entry);
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_STRING("demo", m->name);
  TEST_ASSERT_EQUAL_STRING("1.2.3", m->version);
  TEST_ASSERT_EQUAL_STRING("Tester", m->author);
  TEST_ASSERT_EQUAL_STRING("Demo app", m->description);
  TEST_ASSERT_EQUAL(APP_TYPE_TOOL, m->type);
  TEST_ASSERT_EQUAL((uintptr_t)dummy_entry, m->entry_point);
  TEST_ASSERT_EQUAL(APP_STACK_MEDIUM, m->stack_size);
  TEST_ASSERT_EQUAL(APP_HEAP_SMALL, m->heap_size);
  TEST_ASSERT_TRUE(m->capability_count >= 1);
}

void test_app_request_exit_stops_running_flag(void) {
  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.running = true;

  /* Create a minimal app_desc for a non-home app */
  static app_desc_t test_desc = {
      .name = "test_app",
      .version = "1.0",
      .author = "Test",
      .description = "Test app",
      .type = APP_TYPE_TOOL,
      .fps = 30,
      .stack_size = APP_STACK_SMALL,
      .heap_size = APP_HEAP_SMALL,
  };
  ctx.desc = &test_desc;

  app_request_exit(&ctx);
  TEST_ASSERT_FALSE(ctx.running);
}

void test_app_kit_is_foreground_false_without_focus(void) {
  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  TEST_ASSERT_FALSE(app_kit_is_foreground(&ctx));
  TEST_ASSERT_FALSE(app_kit_is_foreground(NULL));
}

void test_app_bind_key_succeeds_then_rejects_overflow(void) {
  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));

  TEST_ASSERT_EQUAL(0, app_bind_key(&ctx, SIM_KEY_1, dummy_key, NULL));
  TEST_ASSERT_EQUAL(0, app_bind_key(&ctx, SIM_KEY_2, dummy_key, NULL));
  TEST_ASSERT_EQUAL(2, (int)ctx.key_count);

  sim_key_t keys[] = {SIM_KEY_3, SIM_KEY_4, SIM_KEY_5,
                      SIM_KEY_6, SIM_KEY_7, SIM_KEY_8};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    TEST_ASSERT_EQUAL(0, app_bind_key(&ctx, keys[i], dummy_key, NULL));
  }
  TEST_ASSERT_EQUAL(APP_KIT_MAX_KEYS, (int)ctx.key_count);
  TEST_ASSERT_NOT_EQUAL(0, app_bind_key(&ctx, SIM_KEY_9, dummy_key, NULL));
}

static power_demand_t g_during_focus;

static void burst_init(void *raw) {
  app_ctx_t *app = (app_ctx_t *)raw;
  TEST_ASSERT_EQUAL(0, app_kit_set_demand(app, POWER_DEMAND_HIGH));
  g_during_focus = power_governor_get_foreground_demand();
  TEST_ASSERT_EQUAL(POWER_DEMAND_HIGH, power_get_demand(task_get_current()));
  app->running = false;
}

void test_demand_publishes_while_foreground_and_clears_on_exit(void) {
  app_desc_t desc = {
      .name = "burst",
      .fps = 30,
      .on_init = burst_init,
  };
  g_during_focus = POWER_DEMAND_UNSET;
  app_kit_run(&desc);
  TEST_ASSERT_EQUAL(POWER_DEMAND_HIGH, g_during_focus);
  TEST_ASSERT_EQUAL(POWER_DEMAND_NORMAL,
                    power_governor_get_foreground_demand());
  TEST_ASSERT_EQUAL(POWER_DEMAND_NORMAL, power_get_demand(task_get_current()));
}

void test_demand_is_stored_until_the_app_is_foreground(void) {
  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  TEST_ASSERT_EQUAL(0, app_kit_set_demand(&ctx, POWER_DEMAND_LOW));
  TEST_ASSERT_EQUAL(POWER_DEMAND_LOW, app_kit_get_demand(&ctx));
  TEST_ASSERT_EQUAL(POWER_DEMAND_NORMAL,
                    power_governor_get_foreground_demand());
  TEST_ASSERT_EQUAL(-1, app_kit_set_demand(NULL, POWER_DEMAND_LOW));
  TEST_ASSERT_EQUAL(POWER_DEMAND_NORMAL, app_kit_get_demand(NULL));
}

static bool g_copy_foreground;

static void copy_init(void *raw) {
  app_ctx_t *kit = (app_ctx_t *)raw;
  app_ctx_t copy;
  memset(&copy, 0, sizeof(copy));
  copy.desc = kit->desc;
  g_copy_foreground = app_kit_is_foreground(&copy);
  kit->running = false;
}

void test_helper_ctx_counts_as_foreground_when_it_shares_the_desc(void) {
  app_desc_t desc = {
      .name = "copy",
      .fps = 30,
      .on_init = copy_init,
  };
  g_copy_foreground = false;
  app_kit_run(&desc);
  TEST_ASSERT_TRUE(g_copy_foreground);
}

void test_unpaced_app_without_a_timer_sleeps_instead_of_polling(void) {
  app_desc_t desc = {.fps = 30};
  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.desc = &desc;
  TEST_ASSERT_GREATER_THAN(1000, app_kit_next_sleep_ms(&ctx));
}

void test_paced_app_uses_its_frame_clock(void) {
  app_desc_t desc = {.fps = 10};
  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.desc = &desc;
  ctx.paced = true;
  TEST_ASSERT_EQUAL(100, app_kit_next_sleep_ms(&ctx));
}

void test_every_ms_is_the_tool_sleep(void) {
  app_desc_t desc = {.fps = 30};
  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.desc = &desc;
  ctx.every_ms = 500;
  TEST_ASSERT_EQUAL(500, app_kit_next_sleep_ms(&ctx));
}

void test_app_bind_back_registers_escape(void) {
  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  TEST_ASSERT_EQUAL(0, app_bind_back(&ctx));
  TEST_ASSERT_EQUAL(1, (int)ctx.key_count);
  TEST_ASSERT_EQUAL(SIM_KEY_ESCAPE, ctx.keys[0].key);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_app_kit_make_manifest_fills_fields);
  RUN_TEST(test_app_request_exit_stops_running_flag);
  RUN_TEST(test_app_kit_is_foreground_false_without_focus);
  RUN_TEST(test_app_bind_key_succeeds_then_rejects_overflow);
  RUN_TEST(test_demand_publishes_while_foreground_and_clears_on_exit);
  RUN_TEST(test_demand_is_stored_until_the_app_is_foreground);
  RUN_TEST(test_helper_ctx_counts_as_foreground_when_it_shares_the_desc);
  RUN_TEST(test_unpaced_app_without_a_timer_sleeps_instead_of_polling);
  RUN_TEST(test_paced_app_uses_its_frame_clock);
  RUN_TEST(test_every_ms_is_the_tool_sleep);
  RUN_TEST(test_app_bind_back_registers_escape);
  return UNITY_END();
}
