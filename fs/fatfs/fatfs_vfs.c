#include "fatfs_vfs.h"
#include "hal_storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct fatfs_file_handle {
    uint32_t offset;
    uint32_t size;
    char path[VFS_PATH_MAX];
};

struct fatfs_dir_handle {
    int index;
    char path[VFS_PATH_MAX];
};

static fatfs_ctx_t* g_fatfs_ctx = NULL;
static const vfs_ops_t* g_fatfs_ops = NULL;

static int fatfs_mode_to_flags(vfs_mode_t mode) {
    (void)mode;
    return 0;
}

static int fatfs_open(const char* path, vfs_mode_t mode, vfs_file_t** file) {
    fatfs_ctx_t* ctx = g_fatfs_ctx;
    if (!ctx || !ctx->mounted) return -1;
    
    struct fatfs_file_handle* fh = calloc(1, sizeof(struct fatfs_file_handle));
    if (!fh) return -1;
    
    strncpy(fh->path, path, VFS_PATH_MAX - 1);
    fh->offset = 0;
    fh->size = 0;
    
    vfs_file_t* vf = calloc(1, sizeof(vfs_file_t));
    if (!vf) {
        free(fh);
        return -1;
    }
    
    vf->ops = g_fatfs_ops;
    vf->fh = fh;
    *file = vf;
    return 0;
}

static int fatfs_close(vfs_file_t* file) {
    if (!file || !file->fh) return -1;
    struct fatfs_file_handle* fh = (struct fatfs_file_handle*)file->fh;
    free(fh);
    free(file);
    return 0;
}

static ssize_t fatfs_read(vfs_file_t* file, void* buf, size_t count) {
    if (!file || !file->fh) return -1;
    struct fatfs_file_handle* fh = (struct fatfs_file_handle*)file->fh;
    
    fatfs_ctx_t* ctx = g_fatfs_ctx;
    if (!ctx->storage) return -1;
    
    // Simple implementation: read from storage at offset
    int ret = hal_storage_read(ctx->storage, fh->offset, buf, count);
    if (ret == 0) {
        fh->offset += count;
        return count;
    }
    return -1;
}

static ssize_t fatfs_write(vfs_file_t* file, const void* buf, size_t count) {
    if (!file || !file->fh) return -1;
    struct fatfs_file_handle* fh = (struct fatfs_file_handle*)file->fh;
    
    fatfs_ctx_t* ctx = g_fatfs_ctx;
    if (!ctx->storage) return -1;
    
    int ret = hal_storage_write(ctx->storage, fh->offset, buf, count);
    if (ret == 0) {
        fh->offset += count;
        if (fh->offset > fh->size) fh->size = fh->offset;
        return count;
    }
    return -1;
}

static off_t fatfs_seek(vfs_file_t* file, off_t offset, vfs_whence_t whence) {
    if (!file || !file->fh) return -1;
    struct fatfs_file_handle* fh = (struct fatfs_file_handle*)file->fh;
    
    switch (whence) {
        case VFS_SEEK_SET:
            fh->offset = offset;
            break;
        case VFS_SEEK_CUR:
            fh->offset += offset;
            break;
        case VFS_SEEK_END:
            fh->offset = fh->size + offset;
            break;
        default:
            return -1;
    }
    return fh->offset;
}

static off_t fatfs_tell(vfs_file_t* file) {
    if (!file || !file->fh) return -1;
    struct fatfs_file_handle* fh = (struct fatfs_file_handle*)file->fh;
    return fh->offset;
}

static int fatfs_stat(const char* path, vfs_stat_t* st) {
    if (!st) return -1;
    (void)path;
    
    fatfs_ctx_t* ctx = g_fatfs_ctx;
    if (!ctx || !ctx->storage) return -1;
    
    hal_storage_info_t info;
    int ret = hal_storage_get_info(ctx->storage, &info);
    if (ret != 0) return -1;
    
    st->size = info.free_bytes;
    st->blocks = info.free_bytes / info.block_size;
    st->mode = 0100644;
    st->mtime = 0;
    st->ctime = 0;
    st->is_dir = false;
    return 0;
}

static int fatfs_unlink(const char* path) {
    (void)path;
    return -1;
}

static int fatfs_rename(const char* oldpath, const char* newpath) {
    (void)oldpath; (void)newpath;
    return -1;
}

static int fatfs_mkdir(const char* path, uint16_t mode) {
    (void)path; (void)mode;
    return -1;
}

static int fatfs_rmdir(const char* path) {
    (void)path;
    return -1;
}

static vfs_dir_t* fatfs_opendir(const char* path) {
    struct fatfs_dir_handle* dh = calloc(1, sizeof(struct fatfs_dir_handle));
    if (!dh) return NULL;
    
    strncpy(dh->path, path, VFS_PATH_MAX - 1);
    dh->index = 0;
    
    vfs_dir_t* vd = calloc(1, sizeof(vfs_dir_t));
    if (!vd) {
        free(dh);
        return NULL;
    }
    
    vd->ops = g_fatfs_ops;
    vd->dh = dh;
    return vd;
}

static int fatfs_readdir(vfs_dir_t* dir, vfs_dirent_t* entry) {
    if (!dir || !dir->dh || !entry) return -1;
    struct fatfs_dir_handle* dh = (struct fatfs_dir_handle*)dir->dh;
    
    // Simplified - just return empty
    return -1;
}

static int fatfs_closedir(vfs_dir_t* dir) {
    if (!dir || !dir->dh) return -1;
    struct fatfs_dir_handle* dh = (struct fatfs_dir_handle*)dir->dh;
    free(dh);
    free(dir);
    return 0;
}

static int fatfs_sync(vfs_file_t* file) {
    if (!file || !file->fh) return -1;
    fatfs_ctx_t* ctx = g_fatfs_ctx;
    if (!ctx || !ctx->storage) return -1;
    return hal_storage_sync(ctx->storage);
}

static const vfs_ops_t fatfs_ops = {
    .open = fatfs_open,
    .close = fatfs_close,
    .read = fatfs_read,
    .write = fatfs_write,
    .seek = fatfs_seek,
    .tell = fatfs_tell,
    .stat = fatfs_stat,
    .unlink = fatfs_unlink,
    .rename = fatfs_rename,
    .mkdir = fatfs_mkdir,
    .rmdir = fatfs_rmdir,
    .opendir = fatfs_opendir,
    .readdir = fatfs_readdir,
    .closedir = fatfs_closedir,
    .sync = fatfs_sync,
};

const vfs_ops_t* fatfs_get_ops(void) {
    g_fatfs_ops = &fatfs_ops;
    return &fatfs_ops;
}

int fatfs_mount(hal_storage_t* storage, uint8_t pdrv, const char* mount_point) {
    if (!storage || !mount_point) return -1;
    
    fatfs_ctx_t* ctx = calloc(1, sizeof(fatfs_ctx_t));
    if (!ctx) return -1;
    
    ctx->storage = storage;
    ctx->pdrv = pdrv;
    ctx->mounted = true;
    
    g_fatfs_ctx = ctx;
    
    int ret = vfs_mount(mount_point, &fatfs_ops, ctx);
    if (ret != 0) {
        free(ctx);
        g_fatfs_ctx = NULL;
        return -1;
    }
    
    return 0;
}

int fatfs_unmount(const char* mount_point) {
    fatfs_ctx_t* ctx = g_fatfs_ctx;
    if (!ctx) return -1;
    
    int ret = vfs_unmount(mount_point);
    if (ret == 0) {
        free(ctx);
        g_fatfs_ctx = NULL;
        g_fatfs_ops = NULL;
    }
    return ret;
}

int fatfs_format(hal_storage_t* storage, uint8_t pdrv, uint32_t au_size) {
    (void)storage; (void)pdrv; (void)au_size;
    return 0;
}

int fatfs_get_info(const char* mount_point, uint32_t* total, uint32_t* free) {
    fatfs_ctx_t* ctx = g_fatfs_ctx;
    if (!ctx || !ctx->storage || !total || !free) return -1;
    
    hal_storage_info_t info;
    int ret = hal_storage_get_info(ctx->storage, &info);
    if (ret != 0) return -1;
    
    *total = info.total_bytes;
    *free = info.free_bytes;
    return 0;
}