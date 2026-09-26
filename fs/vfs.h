#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_PATH_MAX 128
#define VFS_NAME_MAX 32

typedef enum {
    VFS_SEEK_SET = 0,
    VFS_SEEK_CUR,
    VFS_SEEK_END
} vfs_whence_t;

typedef enum {
    VFS_MODE_READ = 0x01,
    VFS_MODE_WRITE = 0x02,
    VFS_MODE_APPEND = 0x04,
    VFS_MODE_CREATE = 0x08,
    VFS_MODE_TRUNC = 0x10,
    VFS_MODE_EXCL = 0x20
} vfs_mode_t;

typedef struct vfs_stat {
    uint32_t size;
    uint32_t blocks;
    uint16_t mode;
    uint32_t mtime;
    uint32_t ctime;
    bool is_dir;
} vfs_stat_t;

typedef struct vfs_dirent {
    char name[VFS_NAME_MAX];
    uint32_t size;
    bool is_dir;
} vfs_dirent_t;

typedef struct vfs_file {
    const struct vfs_ops* ops;
    void* fh;
} vfs_file_t;

typedef struct vfs_dir {
    const struct vfs_ops* ops;
    void* dh;
} vfs_dir_t;

typedef struct vfs_ops {
    int (*open)(const char* path, vfs_mode_t mode, vfs_file_t** file);
    int (*close)(vfs_file_t* file);
    ssize_t (*read)(vfs_file_t* file, void* buf, size_t count);
    ssize_t (*write)(vfs_file_t* file, const void* buf, size_t count);
    off_t (*seek)(vfs_file_t* file, off_t offset, vfs_whence_t whence);
    off_t (*tell)(vfs_file_t* file);
    int (*stat)(const char* path, vfs_stat_t* st);
    int (*unlink)(const char* path);
    int (*rename)(const char* oldpath, const char* newpath);
    int (*mkdir)(const char* path, uint16_t mode);
    int (*rmdir)(const char* path);
    vfs_dir_t* (*opendir)(const char* path);
    int (*readdir)(vfs_dir_t* dir, vfs_dirent_t* entry);
    int (*closedir)(vfs_dir_t* dir);
    int (*sync)(vfs_file_t* file);
} vfs_ops_t;

typedef struct vfs_mount {
    char prefix[VFS_NAME_MAX];
    const vfs_ops_t* ops;
    void* fs_data;
    struct vfs_mount* next;
} vfs_mount_t;

int vfs_init(void);
void vfs_deinit(void);

int vfs_mount(const char* prefix, const vfs_ops_t* ops, void* fs_data);
int vfs_unmount(const char* prefix);

int vfs_open(const char* path, vfs_mode_t mode, vfs_file_t** file);
int vfs_close(vfs_file_t* file);
ssize_t vfs_read(vfs_file_t* file, void* buf, size_t count);
ssize_t vfs_write(vfs_file_t* file, const void* buf, size_t count);
off_t vfs_seek(vfs_file_t* file, off_t offset, vfs_whence_t whence);
off_t vfs_tell(vfs_file_t* file);
int vfs_stat(const char* path, vfs_stat_t* st);
int vfs_unlink(const char* path);
int vfs_rename(const char* oldpath, const char* newpath);
int vfs_mkdir(const char* path, uint16_t mode);
int vfs_rmdir(const char* path);

vfs_dir_t* vfs_opendir(const char* path);
int vfs_readdir(vfs_dir_t* dir, vfs_dirent_t* entry);
int vfs_closedir(vfs_dir_t* dir);

int vfs_sync(vfs_file_t* file);

const char* vfs_get_mount_point(const char* path);

#ifdef __cplusplus
}
#endif