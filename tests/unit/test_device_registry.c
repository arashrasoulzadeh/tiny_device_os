#include "unity.h"
#include "device_registry.h"
#include <string.h>

/* drivers/device_registry.c had zero tests. It's a separate array-backed
 * device list from driver.c's own linked list - the two aren't kept in
 * sync by anything, since nothing calls device_registry_init() either
 * (same "real code, never wired in" situation as driver.c). Tests use
 * static device_t's throughout (never the registry's own allocation
 * path, hotplug_scan_i2c() - see the leak noted in that function). */

void setUp(void) {
    device_registry_init();
}

void tearDown(void) {
    device_registry_deinit();
}

static device_t make_device(const char* name, const char* path) {
    device_t dev = {0};
    strncpy(dev.name, name, DEVICE_NAME_MAX - 1);
    strncpy(dev.path, path, sizeof(dev.path) - 1);
    return dev;
}

void test_add_find_by_path_and_name(void) {
    device_t dev = make_device("sensor0", "/dev/sensor0");
    TEST_ASSERT_EQUAL(0, device_registry_add(&dev));

    TEST_ASSERT_EQUAL_PTR(&dev, device_registry_find("/dev/sensor0"));
    TEST_ASSERT_EQUAL_PTR(&dev, device_registry_find_by_name("sensor0"));
}

void test_add_duplicate_path_is_rejected(void) {
    device_t a = make_device("a", "/dev/shared");
    device_t b = make_device("b", "/dev/shared");
    TEST_ASSERT_EQUAL(0, device_registry_add(&a));
    TEST_ASSERT_EQUAL(-1, device_registry_add(&b));
}

void test_add_duplicate_name_is_rejected(void) {
    device_t a = make_device("same", "/dev/a");
    device_t b = make_device("same", "/dev/b");
    TEST_ASSERT_EQUAL(0, device_registry_add(&a));
    TEST_ASSERT_EQUAL(-1, device_registry_add(&b));
}

void test_remove_unlinks_and_shifts_remaining(void) {
    device_t a = make_device("a", "/dev/a");
    device_t b = make_device("b", "/dev/b");
    device_t c = make_device("c", "/dev/c");
    device_registry_add(&a);
    device_registry_add(&b);
    device_registry_add(&c);

    TEST_ASSERT_EQUAL(0, device_registry_remove("/dev/b"));
    TEST_ASSERT_NULL(device_registry_find("/dev/b"));
    TEST_ASSERT_EQUAL_PTR(&a, device_registry_find("/dev/a"));
    TEST_ASSERT_EQUAL_PTR(&c, device_registry_find("/dev/c"));

    size_t count;
    device_t** list;
    device_registry_list(&list, &count);
    TEST_ASSERT_EQUAL(2, count);
}

void test_remove_unknown_path_fails(void) {
    TEST_ASSERT_EQUAL(-1, device_registry_remove("/dev/nope"));
}

void test_registry_grows_past_initial_capacity(void) {
    device_t devs[20];
    char paths[20][16];
    for (int i = 0; i < 20; i++) {
        snprintf(paths[i], sizeof(paths[i]), "/dev/d%d", i);
        devs[i] = make_device("dev", paths[i]);
        strncpy(devs[i].name, paths[i] + 5, DEVICE_NAME_MAX - 1);  // unique names too
        TEST_ASSERT_EQUAL(0, device_registry_add(&devs[i]));
    }

    size_t count;
    device_t** list;
    device_registry_list(&list, &count);
    TEST_ASSERT_EQUAL(20, count);
    TEST_ASSERT_NOT_NULL(device_registry_find("/dev/d19"));
}

static int g_hotplug_calls;
static hotplug_event_type_t g_last_type;
static char g_last_path[64];

static void hotplug_cb(const hotplug_event_t* event, void* arg) {
    (void)arg;
    g_hotplug_calls++;
    g_last_type = event->type;
    strncpy(g_last_path, event->device_path, sizeof(g_last_path) - 1);
}

void test_hotplug_callback_fires_on_add_and_remove(void) {
    device_registry_set_hotplug_callback(hotplug_cb, NULL);
    g_hotplug_calls = 0;

    device_t dev = make_device("hp", "/dev/hp");
    device_registry_add(&dev);
    TEST_ASSERT_EQUAL(1, g_hotplug_calls);
    TEST_ASSERT_EQUAL(HOTPLUG_EVENT_ADD, g_last_type);
    TEST_ASSERT_EQUAL_STRING("/dev/hp", g_last_path);

    device_registry_remove("/dev/hp");
    TEST_ASSERT_EQUAL(2, g_hotplug_calls);
    TEST_ASSERT_EQUAL(HOTPLUG_EVENT_REMOVE, g_last_type);
}

void test_emit_hotplug_fires_callback_directly(void) {
    device_registry_set_hotplug_callback(hotplug_cb, NULL);
    g_hotplug_calls = 0;

    device_registry_emit_hotplug(HOTPLUG_EVENT_CHANGE, "/dev/x", "driverX", NULL);
    TEST_ASSERT_EQUAL(1, g_hotplug_calls);
    TEST_ASSERT_EQUAL(HOTPLUG_EVENT_CHANGE, g_last_type);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_find_by_path_and_name);
    RUN_TEST(test_add_duplicate_path_is_rejected);
    RUN_TEST(test_add_duplicate_name_is_rejected);
    RUN_TEST(test_remove_unlinks_and_shifts_remaining);
    RUN_TEST(test_remove_unknown_path_fails);
    RUN_TEST(test_registry_grows_past_initial_capacity);
    RUN_TEST(test_hotplug_callback_fires_on_add_and_remove);
    RUN_TEST(test_emit_hotplug_fires_callback_directly);
    return UNITY_END();
}
