#include "unity.h"
#include "clock_service.h"
#include "hal_storage.h"
#include "littlefs_vfs.h"
#include "vfs.h"

#include <stdlib.h>
#include <string.h>

static hal_storage_t* g_storage;

static time_t read_file(void) {
    vfs_file_t* file = NULL;
    char buf[32];
    ssize_t n;
    if (vfs_open(CLOCK_SERVICE_PATH, VFS_MODE_READ, &file) != 0) {
        return 0;
    }
    n = vfs_read(file, buf, sizeof(buf) - 1);
    vfs_close(file);
    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';
    return (time_t)strtol(buf, NULL, 10);
}

void setUp(void) {
    os_clock_reset();
    vfs_init();
    g_storage = hal_storage_open("/test/flash", HAL_STORAGE_TYPE_FLASH);
    TEST_ASSERT_NOT_NULL(g_storage);
    TEST_ASSERT_EQUAL(0, hal_storage_init(g_storage));
    TEST_ASSERT_EQUAL(0, littlefs_mount(g_storage, 0, 256 * 1024, 4096, "/flash"));
}

void tearDown(void) {
    littlefs_unmount("/flash");
    if (g_storage) {
        hal_storage_deinit(g_storage);
        hal_storage_close(g_storage);
        g_storage = NULL;
    }
    os_clock_reset();
}

void test_empty_storage_saves_the_compile_time(void) {
    TEST_ASSERT_EQUAL(0, clock_service_start(0, 1700000000));
    TEST_ASSERT_EQUAL_INT(1700000000, (int)read_file());
    TEST_ASSERT_INT_WITHIN(2, 1700000000, (int)os_clock_now());
}

void test_saved_time_wins_over_the_compile_stamp(void) {
    vfs_file_t* file = NULL;
    const char* text = "1700002222\n";
    TEST_ASSERT_EQUAL(0, vfs_open(CLOCK_SERVICE_PATH,
                                  VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file));
    TEST_ASSERT_EQUAL((int)strlen(text), (int)vfs_write(file, text, strlen(text)));
    vfs_close(file);

    TEST_ASSERT_EQUAL(0, clock_service_start(0, 1700000000));
    TEST_ASSERT_INT_WITHIN(2, 1700002222, (int)os_clock_now());
    TEST_ASSERT_EQUAL_INT(1700002222, (int)read_file());
}

void test_shift_and_checkpoint_rewrite_storage(void) {
    TEST_ASSERT_EQUAL(0, clock_service_start(0, 1700000000));
    TEST_ASSERT_EQUAL(0, os_clock_shift(30));
    TEST_ASSERT_EQUAL(0, clock_service_checkpoint());
    TEST_ASSERT_EQUAL_INT(1700000000, (int)read_file());

    TEST_ASSERT_EQUAL(0, clock_service_shift(60));
    TEST_ASSERT_INT_WITHIN(2, 1700000090, (int)read_file());
    TEST_ASSERT_INT_WITHIN(2, 1700000090, (int)os_clock_now());
}

void test_a_running_clock_is_left_in_place_and_stored(void) {
    time_t before;
    os_clock_reset();
    before = os_clock_now();
    TEST_ASSERT_EQUAL(0, clock_service_start(before, 1700000000));
    TEST_ASSERT_INT_WITHIN(2, (int)before, (int)os_clock_now());
    TEST_ASSERT_INT_WITHIN(2, (int)before, (int)read_file());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_storage_saves_the_compile_time);
    RUN_TEST(test_saved_time_wins_over_the_compile_stamp);
    RUN_TEST(test_shift_and_checkpoint_rewrite_storage);
    RUN_TEST(test_a_running_clock_is_left_in_place_and_stored);
    return UNITY_END();
}
