#include "unity.h"
#include "fatfs_vfs.h"
#include "vfs.h"
#include "hal_storage.h"
#include <stdio.h>
#include <string.h>

static hal_storage_t* g_test_storage = NULL;

void setUp(void) {
    hal_storage_t* storage = hal_storage_open("/test/sd", HAL_STORAGE_TYPE_SD_SPI);
    TEST_ASSERT_NOT_NULL(storage);
    
    int ret_init = hal_storage_init(storage);
    TEST_ASSERT_EQUAL(0, ret_init);
    
    int ret_mount = fatfs_mount(storage, 0, "/sd");
    TEST_ASSERT_EQUAL(0, ret_mount);
    
    g_test_storage = storage;
}

void tearDown(void) {
    fatfs_unmount("/sd");
    
    if (g_test_storage) {
        hal_storage_deinit(g_test_storage);
        hal_storage_close(g_test_storage);
        g_test_storage = NULL;
    }
}

void test_fatfs_basic_write_read(void) {
    vfs_file_t* file1;
    int ret1 = vfs_open("/sd/test.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file1);
    TEST_ASSERT_EQUAL(0, ret1);
    
    const char* test_data = "Hello, FatFS!";
    ssize_t written = vfs_write(file1, "Hello, FatFS!", strlen("Hello, FatFS!"));
    TEST_ASSERT_EQUAL(strlen("Hello, FatFS!"), written);
    
    int ret1_close = vfs_close(file1);
    TEST_ASSERT_EQUAL(0, ret1_close);
    
    vfs_file_t* file2;
    int ret2 = vfs_open("/sd/test.txt", VFS_MODE_READ, &file2);
    TEST_ASSERT_EQUAL(0, ret2);
    
    char buffer[64] = {0};
    ssize_t read = vfs_read(file2, buffer, 63);
    TEST_ASSERT_GREATER_THAN(0, read);
    TEST_ASSERT_EQUAL_STRING("Hello, FatFS!", buffer);
    
    int ret2_close = vfs_close(file2);
    TEST_ASSERT_EQUAL(0, ret2_close);
}

void test_fatfs_directory_operations(void) {
    int ret_mkdir = vfs_mkdir("/sd/testdir", 0755);
    TEST_ASSERT_EQUAL(0, ret_mkdir);
    
    vfs_file_t* file1;
    int ret1 = vfs_open("/sd/testdir/file1.txt", VFS_MODE_WRITE | VFS_MODE_CREATE, &file1);
    TEST_ASSERT_EQUAL(0, ret1);
    vfs_write(file1, "content1", 8);
    vfs_close(file1);
    
    vfs_file_t* file2;
    int ret2 = vfs_open("/sd/testdir/file2.txt", VFS_MODE_WRITE | VFS_MODE_CREATE, &file2);
    TEST_ASSERT_EQUAL(0, ret2);
    vfs_write(file2, "content2", 8);
    vfs_close(file2);
    
    vfs_dir_t* dir = vfs_opendir("/sd/testdir");
    TEST_ASSERT_NOT_NULL(dir);
    
    vfs_dirent_t entry;
    int count = 0;
    while (vfs_readdir(dir, &entry) == 0) {
        printf("Entry: %s (dir=%d, size=%u)\n", entry.name, entry.is_dir, (unsigned)entry.size);
        count++;
    }
    TEST_ASSERT_EQUAL(2, count);
    
    int ret_closedir = vfs_closedir(dir);
    TEST_ASSERT_EQUAL(0, ret_closedir);
}

void test_fatfs_file_operations(void) {
    vfs_file_t* file1;
    int ret1 = vfs_open("/sd/seek.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file1);
    TEST_ASSERT_EQUAL(0, ret1);
    
    const char* data = "0123456789ABCDEF";
    vfs_write(file1, "0123456789ABCDEF", 16);
    vfs_close(file1);
    
    vfs_file_t* file2;
    int ret2_open = vfs_open("/sd/seek.txt", VFS_MODE_READ, &file2);
    TEST_ASSERT_EQUAL(0, ret2_open);
    
    off_t pos = vfs_tell(file2);
    TEST_ASSERT_EQUAL(0, pos);
    
    char buf[5];
    ssize_t read = vfs_read(file2, buf, 5);
    TEST_ASSERT_EQUAL(5, read);
    
    off_t pos2 = vfs_tell(file2);
    TEST_ASSERT_EQUAL(5, pos2);
    
    int ret2_seek = vfs_seek(file2, 0, VFS_SEEK_SET);
    TEST_ASSERT_EQUAL(0, ret2_seek);
    
    off_t pos3 = vfs_tell(file2);
    TEST_ASSERT_EQUAL(0, pos3);
    
    int ret3 = vfs_seek(file2, -5, VFS_SEEK_END);
    TEST_ASSERT_EQUAL(11, ret3);
    
    vfs_close(file2);
}

void test_fatfs_stat(void) {
    vfs_file_t* file1;
    int ret1 = vfs_open("/sd/stat.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &file1);
    TEST_ASSERT_EQUAL(0, ret1);
    
    const char* data = "Test content for stat";
    vfs_write(file1, data, strlen(data));
    vfs_close(file1);
    
    vfs_stat_t st;
    int ret2 = vfs_stat("/sd/stat.txt", &st);
    TEST_ASSERT_EQUAL(0, ret2);
    TEST_ASSERT_FALSE(st.is_dir);
    TEST_ASSERT_EQUAL(strlen("Test content for stat"), st.size);
}

void test_fatfs_rename_unlink(void) {
    vfs_file_t* file1;
    int ret1 = vfs_open("/sd/original.txt", VFS_MODE_WRITE | VFS_MODE_CREATE, &file1);
    TEST_ASSERT_EQUAL(0, ret1);
    vfs_write(file1, "original", 8);
    vfs_close(file1);
    
    int ret_rename = vfs_rename("/sd/original.txt", "/sd/renamed.txt");
    TEST_ASSERT_EQUAL(0, ret_rename);
    
    vfs_stat_t st;
    int ret2 = vfs_stat("/sd/original.txt", &st);
    TEST_ASSERT_NOT_EQUAL(0, ret2);
    
    int ret3 = vfs_stat("/sd/renamed.txt", &st);
    TEST_ASSERT_EQUAL(0, ret3);
    
    int ret_unlink = vfs_unlink("/sd/renamed.txt");
    TEST_ASSERT_EQUAL(0, ret_unlink);
    
    int ret = vfs_stat("/sd/renamed.txt", &st);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

void test_fatfs_get_info(void) {
    uint32_t total, free;
    int ret = fatfs_get_info("/sd", &total, &free);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_GREATER_THAN(0, total);
    TEST_ASSERT_GREATER_THAN(0, free);
    TEST_ASSERT_GREATER_THAN(total, free);
}

void test_fatfs_format(void) {
    int ret = fatfs_format(NULL, 0, 512);
    TEST_ASSERT_EQUAL(0, ret);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_fatfs_basic_write_read);
    RUN_TEST(test_fatfs_directory_operations);
    RUN_TEST(test_fatfs_file_operations);
    RUN_TEST(test_fatfs_stat);
    RUN_TEST(test_fatfs_rename_unlink);
    RUN_TEST(test_fatfs_get_info);
    RUN_TEST(test_fatfs_format);
    
    return UNITY_END();
}
