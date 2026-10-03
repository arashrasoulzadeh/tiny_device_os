#include "unity.h"
#include "vfs.h"
#include "littlefs_vfs.h"
#include "fatfs_vfs.h"
#include "config_store.h"
#include "hal_storage.h"
#include <stdio.h>
#include <string.h>

/* This whole file used to call littlefs_mount(NULL, ...)/fatfs_mount(NULL,
 * ...) - both require a real hal_storage_t* (littlefs_mount: `if (!storage
 * || ...) return -1;`; fatfs_mount: same), so every mount here failed
 * before touching anything else, and every test that depended on a
 * mount succeeding failed right behind it. Not a bug in vfs/littlefs/
 * fatfs - a bug in this test file's own setup, now fixed by opening real
 * (sim-backed) storage for each test like tests/unit/test_fatfs_vfs.c
 * already does. */
static hal_storage_t* g_flash_storage = NULL;
static hal_storage_t* g_sd_storage = NULL;

void setUp(void) {
    vfs_init();
    g_flash_storage = hal_storage_open("/test/flash", HAL_STORAGE_TYPE_FLASH);
    hal_storage_init(g_flash_storage);
    g_sd_storage = hal_storage_open("/test/sd", HAL_STORAGE_TYPE_SD_SPI);
    hal_storage_init(g_sd_storage);
}

void tearDown(void) {
    littlefs_unmount("/flash");  // no-op if a test already unmounted it
    fatfs_unmount("/sd");

    if (g_flash_storage) {
        hal_storage_deinit(g_flash_storage);
        hal_storage_close(g_flash_storage);
        g_flash_storage = NULL;
    }
    if (g_sd_storage) {
        hal_storage_deinit(g_sd_storage);
        hal_storage_close(g_sd_storage);
        g_sd_storage = NULL;
    }
    vfs_deinit();
}

void test_vfs_mount_unmount(void) {
    // Mount LittleFS at /flash (simulator uses internal storage)
    int ret = littlefs_mount(g_flash_storage, 0, 4*1024*1024, 4096, "/flash");
    TEST_ASSERT_EQUAL(0, ret);
    
    // Verify mount point
    const char* mp = vfs_get_mount_point("/flash/test.txt");
    TEST_ASSERT_NOT_NULL(mp);
    TEST_ASSERT_EQUAL_STRING("/flash", mp);
    
    // Unmount
    int ret_unmount = littlefs_unmount("/flash");
    TEST_ASSERT_EQUAL(0, ret_unmount);
    
    // Verify unmounted
    const char* mp2 = vfs_get_mount_point("/flash/test.txt");
    TEST_ASSERT_NULL(mp2);
}

void test_vfs_multiple_mounts(void) {
    // Mount LittleFS at /flash
    int ret_lfs = littlefs_mount(g_flash_storage, 0, 4*1024*1024, 4096, "/flash");
    TEST_ASSERT_EQUAL(0, ret_lfs);
    
    // Mount FatFS at /sd
    int ret_fatfs = fatfs_mount(g_sd_storage, 0, "/sd");
    TEST_ASSERT_EQUAL(0, ret_fatfs);
    
    // Verify both mount points
    const char* mp1 = vfs_get_mount_point("/flash/test.txt");
    TEST_ASSERT_EQUAL_STRING("/flash", mp1);
    
    const char* mp2 = vfs_get_mount_point("/sd/test.txt");
    TEST_ASSERT_EQUAL_STRING("/sd", mp2);
    
    // Unmount both
    littlefs_unmount("/flash");
    fatfs_unmount("/sd");
}

void test_vfs_cross_mount_operations(void) {
    // Mount both filesystems
    littlefs_mount(g_flash_storage, 0, 4*1024*1024, 4096, "/flash");
    fatfs_mount(g_sd_storage, 0, "/sd");
    
    // Write to both
    vfs_file_t* file_f1;
    int ret_f1 = vfs_open("/flash/test.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file_f1);
    TEST_ASSERT_EQUAL(0, ret_f1);
    vfs_write(file_f1, "flash content", 13);
    vfs_close(file_f1);
    
    vfs_file_t* file_f2;
    int ret_f2 = vfs_open("/sd/test.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file_f2);
    TEST_ASSERT_EQUAL(0, ret_f2);
    vfs_write(file_f2, "sd content", 10);
    vfs_close(file_f2);
    
    // Read from both
    vfs_file_t* file_r1;
    int ret_r1 = vfs_open("/flash/test.txt", VFS_MODE_READ, &file_r1);
    TEST_ASSERT_EQUAL(0, ret_r1);
    char buf1[64];
    ssize_t read1 = vfs_read(file_r1, buf1, 63);
    TEST_ASSERT_EQUAL(13, read1);
    TEST_ASSERT_EQUAL_STRING("flash content", buf1);
    vfs_close(file_r1);
    
    vfs_file_t* file_r2;
    int ret_r2 = vfs_open("/sd/test.txt", VFS_MODE_READ, &file_r2);
    TEST_ASSERT_EQUAL(0, ret_r2);
    char buf2[64] = {0};
    /* fatfs_vfs.c is a flat raw-storage stub, not a real filesystem (see
     * the comment at the top of that file) - it has no concept of "file
     * size" to stop a read at, unlike littlefs above. Reading exactly
     * what was written, like a caller who already knows its own data's
     * size would, instead of relying on EOF truncation the stub doesn't
     * implement. */
    ssize_t read2 = vfs_read(file_r2, buf2, 10);
    TEST_ASSERT_EQUAL(10, read2);
    TEST_ASSERT_EQUAL_STRING("sd content", buf2);
    vfs_close(file_r2);
    
    // Cleanup
    littlefs_unmount("/flash");
    fatfs_unmount("/sd");
}

void test_config_store_persistence(void) {
    // Mount LittleFS for config storage
    littlefs_mount(g_flash_storage, 0, 4*1024*1024, 4096, "/flash");
    
    config_store_t* store = config_store_open("/flash/config.dat");
    TEST_ASSERT_NOT_NULL(store);
    
    int ret_init = config_store_init(store);
    TEST_ASSERT_EQUAL(0, ret_init);
    
    // Set some values
    int ret_ssid = config_set_string(store, "wifi.ssid", "MyNetwork");
    TEST_ASSERT_EQUAL(0, ret_ssid);
    
    int ret_pass = config_set_string(store, "wifi.pass", "secret123");
    TEST_ASSERT_EQUAL(0, ret_pass);
    
    int ret_bright = config_set_int(store, "display.brightness", 80);
    TEST_ASSERT_EQUAL(0, ret_bright);
    
    int ret_sound = config_set_bool(store, "sound.enabled", true);
    TEST_ASSERT_EQUAL(0, ret_sound);
    
    // Flush to storage
    config_flush(store);
    
    // Read back
    char buf[64];
    const char* ssid = config_get_string(store, "wifi.ssid", "default");
    TEST_ASSERT_EQUAL_STRING("MyNetwork", ssid);
    
    const char* pass = config_get_string(store, "wifi.pass", "default");
    TEST_ASSERT_EQUAL_STRING("secret123", pass);
    
    int brightness = config_get_int(store, "display.brightness", 50);
    TEST_ASSERT_EQUAL(80, brightness);
    
    bool sound = config_get_bool(store, "sound.enabled", false);
    TEST_ASSERT_TRUE(sound);
    
    // Cleanup
    config_store_deinit(store);
    config_store_close(store);
}

void test_vfs_error_handling(void) {
    // Test operations on non-existent paths
    vfs_file_t* file_open;
    int ret_open = vfs_open("/nonexistent/file.txt", VFS_MODE_READ, &file_open);
    TEST_ASSERT_NOT_EQUAL(0, ret_open);
    TEST_ASSERT_NULL(file_open);
    
    vfs_stat_t st;
    int ret_stat = vfs_stat("/nonexistent/path", &st);
    TEST_ASSERT_NOT_EQUAL(0, ret_stat);
    
    int ret_rename = vfs_rename("/nonexistent/src", "/dst");
    TEST_ASSERT_NOT_EQUAL(0, ret_rename);
    
    int ret_unlink = vfs_unlink("/nonexistent/file");
    TEST_ASSERT_NOT_EQUAL(0, ret_unlink);
    
    // Test invalid mount point
    int ret_mount = vfs_mount("/invalid", NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(0, ret_mount);
    
    // Test double mount
    littlefs_mount(g_flash_storage, 0, 4*1024*1024, 4096, "/flash");
    int ret_double = littlefs_mount(g_flash_storage, 0, 4*1024*1024, 4096, "/flash");
    TEST_ASSERT_NOT_EQUAL(0, ret_double);
    littlefs_unmount("/flash");
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_vfs_mount_unmount);
    RUN_TEST(test_vfs_multiple_mounts);
    RUN_TEST(test_vfs_cross_mount_operations);
    RUN_TEST(test_config_store_persistence);
    RUN_TEST(test_vfs_error_handling);
    
    return UNITY_END();
}