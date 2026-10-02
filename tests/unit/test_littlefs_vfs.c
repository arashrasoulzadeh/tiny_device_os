#include "unity.h"
#include "littlefs_vfs.h"
#include "vfs.h"
#include "hal_storage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static lfs_fs_ctx_t* g_test_ctx = NULL;
static hal_storage_t* g_test_storage = NULL;

void setUp(void) {
    hal_storage_t* storage = hal_storage_open("/test/flash", HAL_STORAGE_TYPE_FLASH);
    TEST_ASSERT_NOT_NULL(storage);
    
    int ret_init = hal_storage_init(storage);
    TEST_ASSERT_EQUAL(0, ret_init);
    
    /* Use smaller size (256KB) for faster testing */
    int ret_mount = littlefs_mount(storage, 0, 256*1024, 4096, "/flash");
    TEST_ASSERT_EQUAL(0, ret_mount);
    
    g_test_storage = storage;
}

void tearDown(void) {
    littlefs_unmount("/flash");
    
    if (g_test_storage) {
        hal_storage_deinit(g_test_storage);
        hal_storage_close(g_test_storage);
        g_test_storage = NULL;
    }
}

void test_littlefs_basic_write_read(void) {
    vfs_file_t* file_a;
    int ret_a = vfs_open("/flash/test.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file_a);
    TEST_ASSERT_EQUAL(0, ret_a);
    
    const char* test_data = "Hello, LittleFS!";
    ssize_t written_a = vfs_write(file_a, "Hello, LittleFS!", strlen("Hello, LittleFS!"));
    TEST_ASSERT_EQUAL(strlen("Hello, LittleFS!"), written_a);
    
    int ret_a_close = vfs_close(file_a);
    TEST_ASSERT_EQUAL(0, ret_a_close);
    
    vfs_file_t* file_b;
    int ret_b = vfs_open("/flash/test.txt", VFS_MODE_READ, &file_b);
    TEST_ASSERT_EQUAL(0, ret_b);
    
    char buffer_b[64] = {0};
    ssize_t read_b = vfs_read(file_b, buffer_b, 63);
    TEST_ASSERT_GREATER_THAN(0, read_b);
    TEST_ASSERT_EQUAL_STRING("Hello, LittleFS!", buffer_b);
    
    int ret_b_close = vfs_close(file_b);
    TEST_ASSERT_EQUAL(0, ret_b_close);
}

void test_littlefs_file_operations(void) {
    vfs_file_t* file_w1;
    int ret_w1 = vfs_open("/flash/test2.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_APPEND, &file_w1);
    TEST_ASSERT_EQUAL(0, ret_w1);
    
    vfs_write(file_w1, "Line 1\n", 7);
    vfs_write(file_w1, "Line 2\n", 7);
    vfs_close(file_w1);
    
    vfs_file_t* file_w2;
    int ret_w2 = vfs_open("/flash/test2.txt", VFS_MODE_WRITE | VFS_MODE_APPEND, &file_w2);
    TEST_ASSERT_EQUAL(0, ret_w2);
    
    vfs_write(file_w2, "Line 3\n", 7);
    vfs_close(file_w2);
    
    vfs_file_t* file_r;
    int ret_r = vfs_open("/flash/test2.txt", VFS_MODE_READ, &file_r);
    TEST_ASSERT_EQUAL(0, ret_r);
    
    char buffer_r[128] = {0};
    ssize_t total_read = 0;
    char buf_r[64];
    int ret_read;
    while ((ret_read = vfs_read(file_r, buf_r, sizeof(buf_r)-1)) > 0) {
        buf_r[ret_read] = '\0';
        printf("%s", buf_r);
        total_read += ret_read;
    }
    TEST_ASSERT_GREATER_THAN(0, total_read);
    vfs_close(file_r);
}

void test_littlefs_directory_operations(void) {
    int ret_mkdir = vfs_mkdir("/flash/testdir", 0755);
    TEST_ASSERT_EQUAL(0, ret_mkdir);
    
    vfs_file_t* file_d1;
    int ret_d1 = vfs_open("/flash/testdir/file1.txt", VFS_MODE_WRITE | VFS_MODE_CREATE, &file_d1);
    TEST_ASSERT_EQUAL(0, ret_d1);
    vfs_write(file_d1, "content1", 8);
    vfs_close(file_d1);
    
    vfs_file_t* file_d2;
    int ret_d2 = vfs_open("/flash/testdir/file2.txt", VFS_MODE_WRITE | VFS_MODE_CREATE, &file_d2);
    TEST_ASSERT_EQUAL(0, ret_d2);
    vfs_write(file_d2, "content2", 8);
    vfs_close(file_d2);
    
    vfs_dir_t* dir_d = vfs_opendir("/flash/testdir");
    TEST_ASSERT_NOT_NULL(dir_d);
    
    vfs_dirent_t entry_d;
    int count_d = 0;
    while (vfs_readdir(dir_d, &entry_d) == 0) {
        printf("Entry: %s (dir=%d, size=%u)\n", entry_d.name, entry_d.is_dir, (unsigned)entry_d.size);
        count_d++;
    }
    TEST_ASSERT_EQUAL(2, count_d);
    
    int ret_closedir = vfs_closedir(dir_d);
    TEST_ASSERT_EQUAL(0, ret_closedir);
}

void test_littlefs_file_seek_tell(void) {
    vfs_file_t* file_s1;
    int ret_s1 = vfs_open("/flash/seek.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file_s1);
    TEST_ASSERT_EQUAL(0, ret_s1);
    
    const char* data_s = "0123456789ABCDEF";
    vfs_write(file_s1, "0123456789ABCDEF", 16);
    vfs_close(file_s1);
    
    vfs_file_t* file_s2;
    int ret_s2 = vfs_open("/flash/seek.txt", VFS_MODE_READ, &file_s2);
    TEST_ASSERT_EQUAL(0, ret_s2);
    
    off_t pos_s1 = vfs_tell(file_s2);
    TEST_ASSERT_EQUAL(0, pos_s1);
    
    char buf_s[5];
    ssize_t read_s1 = vfs_read(file_s2, buf_s, 5);
    TEST_ASSERT_EQUAL(5, read_s1);
    
    off_t pos2_s = vfs_tell(file_s2);
    TEST_ASSERT_EQUAL(5, pos2_s);
    
    int ret_seek = vfs_seek(file_s2, 0, VFS_SEEK_SET);
    TEST_ASSERT_EQUAL(0, ret_seek);
    
    off_t pos3_s = vfs_tell(file_s2);
    TEST_ASSERT_EQUAL(0, pos3_s);
    
    int ret3 = vfs_seek(file_s2, -5, VFS_SEEK_END);
    TEST_ASSERT_EQUAL(11, ret3);
    
    vfs_close(file_s2);
}

void test_littlefs_stat(void) {
    vfs_file_t* file_stat1;
    int ret_stat1 = vfs_open("/flash/stat.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file_stat1);
    TEST_ASSERT_EQUAL(0, ret_stat1);
    
    const char* data_stat = "Test content for stat";
    vfs_write(file_stat1, data_stat, strlen(data_stat));
    vfs_close(file_stat1);
    
    vfs_stat_t st;
    int ret_stat2 = vfs_stat("/flash/stat.txt", &st);
    TEST_ASSERT_EQUAL(0, ret_stat2);
    TEST_ASSERT_FALSE(st.is_dir);
    TEST_ASSERT_EQUAL(strlen("Test content for stat"), st.size);
}

void test_littlefs_rename_unlink(void) {
    vfs_file_t* file_rename1;
    int ret_rename1 = vfs_open("/flash/original.txt", VFS_MODE_WRITE | VFS_MODE_CREATE, &file_rename1);
    TEST_ASSERT_EQUAL(0, ret_rename1);
    vfs_write(file_rename1, "original", 8);
    vfs_close(file_rename1);
    
    int ret_rename = vfs_rename("/flash/original.txt", "/flash/renamed.txt");
    TEST_ASSERT_EQUAL(0, ret_rename);
    
    vfs_stat_t st_rename;
    int ret_rename2 = vfs_stat("/flash/original.txt", &st_rename);
    TEST_ASSERT_NOT_EQUAL(0, ret_rename2);
    
    int ret3 = vfs_stat("/flash/renamed.txt", &st_rename);
    TEST_ASSERT_EQUAL(0, ret3);
    
    int ret_unlink = vfs_unlink("/flash/renamed.txt");
    TEST_ASSERT_EQUAL(0, ret_unlink);
    
    int ret_final = vfs_stat("/flash/renamed.txt", &st_rename);
    TEST_ASSERT_NOT_EQUAL(0, ret_final);
}

void test_littlefs_persistence(void) {
    vfs_file_t* file_p1;
    int ret_p1 = vfs_open("/flash/persist.txt", VFS_MODE_WRITE | VFS_MODE_CREATE, &file_p1);
    TEST_ASSERT_EQUAL(0, ret_p1);
    vfs_write(file_p1, "persistent data", 15);
    vfs_close(file_p1);
    
    littlefs_unmount("/flash");
    
    int ret_remount = littlefs_mount(NULL, 0, 256*1024, 4096, "/flash");
    TEST_ASSERT_EQUAL(0, ret_remount);
    
    vfs_file_t* file_p2;
    int ret_p2 = vfs_open("/flash/persist.txt", VFS_MODE_READ, &file_p2);
    TEST_ASSERT_EQUAL(0, ret_p2);
    
    char buffer_p[32] = {0};
    ssize_t read_p = vfs_read(file_p2, buffer_p, 31);
    TEST_ASSERT_EQUAL(15, read_p);
    TEST_ASSERT_EQUAL_STRING("persistent data", buffer_p);
    vfs_close(file_p2);
    
    littlefs_unmount("/flash");
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_littlefs_basic_write_read);
    RUN_TEST(test_littlefs_file_operations);
    RUN_TEST(test_littlefs_directory_operations);
    RUN_TEST(test_littlefs_file_seek_tell);
    RUN_TEST(test_littlefs_stat);
    RUN_TEST(test_littlefs_rename_unlink);
    RUN_TEST(test_littlefs_persistence);
    
    return UNITY_END();
}