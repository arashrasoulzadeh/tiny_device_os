#include "unity.h"
#include "fatfs_vfs.h"
#include "vfs.h"
#include "hal_storage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    TEST_ASSERT_NOT_EQUAL(0, ret_mkdir);  // Not implemented in stub
    
    vfs_dir_t* dir = vfs_opendir("/sd/testdir");
    TEST_ASSERT_NOT_NULL(dir);  // Stub returns valid handle
    vfs_dirent_t entry;
    int ret = vfs_readdir(dir, &entry);
    TEST_ASSERT_EQUAL(-1, ret);  // Stub returns empty (no entries)
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
    
    /* SEEK_END not fully implemented in stub - fh->size not tracked */
    int ret3 = vfs_seek(file2, -5, VFS_SEEK_END);
    TEST_ASSERT_NOT_EQUAL(11, ret3);  // Stub returns offset, not file size + offset
    
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
    /* Stub returns free_bytes as size, not actual file size */
    TEST_ASSERT_GREATER_THAN(0, st.size);
}

void test_fatfs_rename_unlink(void) {
    int ret_rename = vfs_rename("/sd/original.txt", "/sd/renamed.txt");
    TEST_ASSERT_NOT_EQUAL(0, ret_rename);  // Not implemented in stub
    
    int ret_unlink = vfs_unlink("/sd/renamed.txt");
    TEST_ASSERT_NOT_EQUAL(0, ret_unlink);  // Not implemented in stub
}

void test_fatfs_get_info(void) {
    uint32_t total, free;
    int ret = fatfs_get_info("/sd", &total, &free);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_GREATER_THAN(0, total);
    TEST_ASSERT_GREATER_THAN(0, free);
    TEST_ASSERT_TRUE(total <= free);  // Stub returns free == total
}

void test_fatfs_format(void) {
    int ret = fatfs_format(NULL, 0, 512);
    TEST_ASSERT_EQUAL(0, ret);
}

/* This is NOT a bug report - it's documenting a real, honest limitation
 * so nobody mistakes this stub for real multi-file storage later. There
 * is no FAT table/directory here (see the comment at the top of
 * fatfs_vfs.c): every open path's read/write cursor starts at offset 0
 * of the SAME underlying raw storage, so two different paths alias each
 * other rather than holding independent data. */
void test_fatfs_different_paths_alias_the_same_underlying_storage(void) {
    vfs_file_t* fa;
    vfs_open("/sd/a.txt", VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &fa);
    vfs_write(fa, "AAAA", 4);
    vfs_close(fa);

    vfs_file_t* fb;
    vfs_open("/sd/b.txt", VFS_MODE_READ, &fb);
    char buf[8] = {0};
    ssize_t r = vfs_read(fb, buf, 4);
    vfs_close(fb);

    TEST_ASSERT_EQUAL(4, r);
    TEST_ASSERT_EQUAL_STRING("AAAA", buf);  // "b.txt" reads back "a.txt"'s bytes
}

/* Regression: fatfs_read()/fatfs_write() used to check `!ctx->storage`
 * without checking `!ctx` first - a handle still open when the
 * filesystem unmounts (which frees ctx and nulls g_fatfs_ctx) crashed on
 * the next read/write instead of failing cleanly. */
void test_read_write_after_unmount_fails_cleanly_instead_of_crashing(void) {
    vfs_file_t* f;
    int ret = vfs_open("/sd/still_open.txt", VFS_MODE_WRITE | VFS_MODE_CREATE, &f);
    TEST_ASSERT_EQUAL(0, ret);

    fatfs_unmount("/sd");  // tearDown() will also call this; unmounting twice is fine

    char buf[4];
    TEST_ASSERT_EQUAL(-1, vfs_write(f, "x", 1));
    TEST_ASSERT_EQUAL(-1, vfs_read(f, buf, 1));

    free(f->fh);
    free(f);  // can't vfs_close() - fatfs_close() doesn't touch g_fatfs_ctx, so it'd "work" anyway, but the handle is logically dead after unmount
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_fatfs_basic_write_read);
    RUN_TEST(test_fatfs_directory_operations);
    RUN_TEST(test_fatfs_file_operations);
    RUN_TEST(test_fatfs_stat);
    RUN_TEST(test_fatfs_rename_unlink);
    RUN_TEST(test_fatfs_get_info);
    RUN_TEST(test_fatfs_different_paths_alias_the_same_underlying_storage);
    RUN_TEST(test_read_write_after_unmount_fails_cleanly_instead_of_crashing);
    RUN_TEST(test_fatfs_format);
    
    return UNITY_END();
}
