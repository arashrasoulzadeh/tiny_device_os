#include "app.h"
#include "app_kit.h"
#include "os_boot.h"
#include "unity.h"

#include <string.h>
#include <time.h>

static int g_storage_calls;
static int g_load_calls;

static int mount_storage(void) {
  g_storage_calls++;
  return 0;
}

static void heavy_load(void) { g_load_calls++; }

void setUp(void) {
  g_storage_calls = 0;
  g_load_calls = 0;
  app_init();
}

void tearDown(void) { app_deinit(); }

void test_os_boot_load_mounts_storage_and_loads_each_app_once(void) {
  app_manifest_t heavy;
  app_manifest_t quiet;
  os_boot_args_t args;

  memset(&heavy, 0, sizeof(heavy));
  memset(&quiet, 0, sizeof(quiet));
  memcpy(heavy.name, "boot_heavy", 11);
  memcpy(quiet.name, "boot_quiet", 11);
  TEST_ASSERT_EQUAL(0, app_install_manifest(&heavy, "boot_heavy"));
  TEST_ASSERT_EQUAL(0, app_install_manifest(&quiet, "boot_quiet"));
  app_kit_set_load("boot_heavy", heavy_load);

  memset(&args, 0, sizeof(args));
  args.mount_storage = mount_storage;
  args.clock_now = time(NULL);
  TEST_ASSERT_EQUAL(0, os_boot_load(&args));
  TEST_ASSERT_EQUAL(1, g_storage_calls);
  TEST_ASSERT_EQUAL(1, g_load_calls);

  TEST_ASSERT_EQUAL(0, os_boot_load(&args));
  TEST_ASSERT_EQUAL(2, g_storage_calls);
  TEST_ASSERT_EQUAL(1, g_load_calls);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_os_boot_load_mounts_storage_and_loads_each_app_once);
  return UNITY_END();
}
