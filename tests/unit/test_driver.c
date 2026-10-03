#include "unity.h"
#include "driver.h"
#include <string.h>

/* drivers/driver.c had zero tests, and is never actually wired into the
 * real boot path (nothing calls driver_core_init() anywhere outside this
 * file and the per-device driver files under drivers/ - the sim/real
 * apps go through the hal/sim layer instead). Found two real bugs while
 * writing this: device_open()/
 * device_suspend()/device_resume() deadlocked (double-locking a
 * non-reentrant spinlock via device_find_by_path(), which locks itself),
 * and device_close/read/write/ioctl had no way to tell which device a
 * handle belonged to, so they just tried every registered device's op in
 * turn - with two devices open at once, a call could silently execute
 * against the wrong one. Both fixed; this file is the regression test. */

void setUp(void) {
    driver_core_init();
}

void tearDown(void) {
    driver_core_deinit();
}

/* A trivial driver whose "handle" is just the device_t* itself, and whose
 * read/write encode which device answered into the buffer - that's what
 * lets test_read_write_dispatch_to_the_correct_device below tell two
 * devices of the same driver apart. */
static int mock_open(device_t* dev, void** handle) {
    *handle = dev;
    return 0;
}
static int mock_close(void* handle) {
    (void)handle;
    return 0;
}
static ssize_t mock_read(void* handle, void* buf, size_t count) {
    device_t* dev = (device_t*)handle;
    if (count < 1) return -1;
    ((char*)buf)[0] = dev->name[0];
    return 1;
}
static ssize_t mock_write(void* handle, const void* buf, size_t count) {
    (void)handle; (void)buf;
    return (ssize_t)count;
}
static int mock_ioctl(void* handle, uint32_t cmd, void* arg) {
    (void)handle; (void)arg;
    return (int)cmd;
}
static int probe_calls;
static int remove_calls;
static int mock_probe(device_t* dev) { (void)dev; probe_calls++; return 0; }
static int mock_remove(device_t* dev) { (void)dev; remove_calls++; return 0; }

static const driver_ops_t g_mock_ops = {
    .probe = mock_probe, .remove = mock_remove,
    .open = mock_open, .close = mock_close,
    .read = mock_read, .write = mock_write, .ioctl = mock_ioctl,
};

static driver_t g_mock_driver;
static void init_mock_driver(const char* name) {
    memset(&g_mock_driver, 0, sizeof(g_mock_driver));
    strncpy(g_mock_driver.name, name, DRIVER_NAME_MAX - 1);
    g_mock_driver.type = DRIVER_TYPE_CHAR;
    g_mock_driver.ops = &g_mock_ops;
}

void test_register_find_unregister_driver(void) {
    init_mock_driver("mock0");
    TEST_ASSERT_EQUAL(0, driver_register(&g_mock_driver));
    TEST_ASSERT_EQUAL_PTR(&g_mock_driver, driver_find("mock0"));
    TEST_ASSERT_EQUAL(0, driver_unregister("mock0"));
    TEST_ASSERT_NULL(driver_find("mock0"));
}

void test_register_duplicate_driver_name_fails(void) {
    init_mock_driver("mock0");
    TEST_ASSERT_EQUAL(0, driver_register(&g_mock_driver));

    driver_t second;
    memcpy(&second, &g_mock_driver, sizeof(second));
    TEST_ASSERT_EQUAL(-1, driver_register(&second));
}

void test_device_register_calls_probe(void) {
    init_mock_driver("mock0");
    driver_register(&g_mock_driver);

    device_t dev = {0};
    strncpy(dev.name, "dev0", DEVICE_NAME_MAX - 1);
    strncpy(dev.path, "/dev/mock0", sizeof(dev.path) - 1);
    dev.driver = &g_mock_driver;

    probe_calls = 0;
    TEST_ASSERT_EQUAL(0, device_register(&dev));
    TEST_ASSERT_EQUAL(1, probe_calls);
    TEST_ASSERT_EQUAL(1, g_mock_driver.refcount);
    TEST_ASSERT_EQUAL_PTR(&dev, device_find_by_path("/dev/mock0"));
}

void test_device_unregister_calls_remove_and_decrements_refcount(void) {
    init_mock_driver("mock0");
    driver_register(&g_mock_driver);

    device_t dev = {0};
    strncpy(dev.name, "dev0", DEVICE_NAME_MAX - 1);
    strncpy(dev.path, "/dev/mock0", sizeof(dev.path) - 1);
    dev.driver = &g_mock_driver;
    device_register(&dev);

    remove_calls = 0;
    TEST_ASSERT_EQUAL(0, device_unregister("dev0"));
    TEST_ASSERT_EQUAL(1, remove_calls);
    TEST_ASSERT_EQUAL(0, g_mock_driver.refcount);
    TEST_ASSERT_NULL(device_find("dev0"));
}

/* Regression: device_open()/device_suspend()/device_resume() used to
 * double-lock via device_find_by_path() and hang forever. If this test
 * completes at all (rather than timing out the whole suite), it's fixed. */
void test_device_open_does_not_deadlock(void) {
    init_mock_driver("mock0");
    driver_register(&g_mock_driver);

    device_t dev = {0};
    strncpy(dev.name, "dev0", DEVICE_NAME_MAX - 1);
    strncpy(dev.path, "/dev/mock0", sizeof(dev.path) - 1);
    dev.driver = &g_mock_driver;
    device_register(&dev);

    void* handle = NULL;
    TEST_ASSERT_EQUAL(0, device_open("/dev/mock0", &handle));
    TEST_ASSERT_NOT_NULL(handle);
}

void test_device_suspend_resume_do_not_deadlock(void) {
    init_mock_driver("mock0");
    driver_register(&g_mock_driver);

    device_t dev = {0};
    strncpy(dev.name, "dev0", DEVICE_NAME_MAX - 1);
    strncpy(dev.path, "/dev/mock0", sizeof(dev.path) - 1);
    dev.driver = &g_mock_driver;
    device_register(&dev);

    // g_mock_ops has no suspend/resume - this just has to return, not hang.
    TEST_ASSERT_EQUAL(-1, device_suspend("/dev/mock0"));
    TEST_ASSERT_EQUAL(-1, device_resume("/dev/mock0"));
}

/* Regression: device_read/write/close/ioctl used to try every registered
 * device's op in turn and use whichever call "succeeded," regardless of
 * whether the handle actually belonged to that device. Two devices of the
 * same driver open at once is exactly the scenario that broke: a read
 * with device B's handle could return device A's data. */
void test_read_write_dispatch_to_the_correct_device(void) {
    init_mock_driver("mock0");
    driver_register(&g_mock_driver);

    device_t dev_a = {0}, dev_b = {0};
    strncpy(dev_a.name, "A", DEVICE_NAME_MAX - 1);
    strncpy(dev_a.path, "/dev/a", sizeof(dev_a.path) - 1);
    dev_a.driver = &g_mock_driver;
    strncpy(dev_b.name, "B", DEVICE_NAME_MAX - 1);
    strncpy(dev_b.path, "/dev/b", sizeof(dev_b.path) - 1);
    dev_b.driver = &g_mock_driver;

    device_register(&dev_a);
    device_register(&dev_b);

    void *handle_a = NULL, *handle_b = NULL;
    device_open("/dev/a", &handle_a);
    device_open("/dev/b", &handle_b);
    TEST_ASSERT_NOT_EQUAL(handle_a, handle_b);

    char buf = 0;
    TEST_ASSERT_EQUAL(1, device_read(handle_a, &buf, 1));
    TEST_ASSERT_EQUAL('A', buf);

    TEST_ASSERT_EQUAL(1, device_read(handle_b, &buf, 1));
    TEST_ASSERT_EQUAL('B', buf);
}

void test_close_untracks_the_handle(void) {
    init_mock_driver("mock0");
    driver_register(&g_mock_driver);

    device_t dev = {0};
    strncpy(dev.name, "dev0", DEVICE_NAME_MAX - 1);
    strncpy(dev.path, "/dev/mock0", sizeof(dev.path) - 1);
    dev.driver = &g_mock_driver;
    device_register(&dev);

    void* handle = NULL;
    device_open("/dev/mock0", &handle);
    TEST_ASSERT_EQUAL(0, device_close(handle));

    // Closed handle is no longer tracked, so a read against it fails clean
    // instead of dispatching to some other device as a lucky guess.
    char buf;
    TEST_ASSERT_EQUAL(-1, device_read(handle, &buf, 1));
}

void test_unknown_handle_is_rejected_not_guessed(void) {
    init_mock_driver("mock0");
    driver_register(&g_mock_driver);
    device_t dev = {0};
    strncpy(dev.name, "dev0", DEVICE_NAME_MAX - 1);
    strncpy(dev.path, "/dev/mock0", sizeof(dev.path) - 1);
    dev.driver = &g_mock_driver;
    device_register(&dev);

    int not_a_real_handle = 0;
    char buf;
    TEST_ASSERT_EQUAL(-1, device_read(&not_a_real_handle, &buf, 1));
    TEST_ASSERT_EQUAL(-1, device_write(&not_a_real_handle, &buf, 1));
    TEST_ASSERT_EQUAL(-1, device_ioctl(&not_a_real_handle, 0, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_register_find_unregister_driver);
    RUN_TEST(test_register_duplicate_driver_name_fails);
    RUN_TEST(test_device_register_calls_probe);
    RUN_TEST(test_device_unregister_calls_remove_and_decrements_refcount);
    RUN_TEST(test_device_open_does_not_deadlock);
    RUN_TEST(test_device_suspend_resume_do_not_deadlock);
    RUN_TEST(test_read_write_dispatch_to_the_correct_device);
    RUN_TEST(test_close_untracks_the_handle);
    RUN_TEST(test_unknown_handle_is_rejected_not_guessed);
    return UNITY_END();
}
