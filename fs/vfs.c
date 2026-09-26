#include "vfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_MOUNTS 8

static vfs_mount_t* g_mounts = NULL;
static int g_mount_count = 0;

static vfs_mount_t* vfs_find_mount(const char* path, const char** rel_path) {
    vfs_mount_t* best = NULL;
    size_t best_len = 0;
    
    for (vfs_mount_t* m = g_mounts; m; m = m->next) {
        size_t len = strlen(m->prefix);
        if (strncmp(path, m->prefix, len) == 0 && 
            (path[len] == '/' || path[len] == '\0') && 
            len > best_len) {
            best = m;
            best_len = len;
        }
    }
    
    if (best && rel_path) {
        *rel_path = path + best_len;
        if (**rel_path == '/') (*rel_path)++;
    }
    
    return best;
}

int vfs_init(void) {
    g_mounts = NULL;
    g_mount_count = 0;
    return 0;
}

void vfs_deinit(void) {
    while (g_mounts) {
        vfs_mount_t* m = g_mounts;
        g_mounts = m->next;
        free(m);
    }
    g_mount_count = 0;
}

int vfs_mount(const char* prefix, const vfs_ops_t* ops, void* fs_data) {
    if (!prefix || !ops || g_mount_count >= MAX_MOUNTS) return -1;
    
    for (vfs_mount_t* m = g_mounts; m; m = m->next) {
        if (strcmp(m->prefix, prefix) == 0) return -1;
    }
    
    vfs_mount_t* mount = calloc(1, sizeof(vfs_mount_t));
    if (!mount) return -1;
    
    strncpy(mount->prefix, prefix, VFS_NAME_MAX - 1);
    mount->ops = ops;
    mount->fs_data = fs_data;
    mount->next = g_mounts;
    g_mounts = mount;
    g_mount_count++;
    
    return 0;
}

int vfs_unmount(const char* prefix) {
    vfs_mount_t** prev = &g_mounts;
    for (vfs_mount_t* m = g_mounts; m; m = m->next) {
        if (strcmp(m->prefix, prefix) == 0) {
            *prev = m->next;
            free(m);
            g_mount_count--;
            return 0;
        }
        prev = &m->next;
    }
    return -1;
}

static int vfs_do_open(const char* path, vfs_mode_t mode, vfs_file_t** file) {
    const char* rel_path;
    vfs_mount_t* mount = vfs_find_mount(path, &rel_path);
    if (!mount || !mount->ops->open) return -1;
    
    vfs_file_t* f = calloc(1, sizeof(vfs_file_t));
    if (!f) return -1;
    
    f->ops = mount->ops;
    int ret = mount->ops->open(rel_path, mode, &f->fh);
    if (ret != 0) {
        free(f);
        return ret;
    }
    
    *file = f;
    return 0;
}

int vfs_open(const char* path, vfs_mode_t mode, vfs_file_t** file) {
    if (!path || !file) return -1;
    return vfs_do_open(path, mode, file);
}

int vfs_close(vfs_file_t* file) {
    if (!file || !file->ops || !file->ops->close) return -1;
    int ret = file->ops->close(file->fh);
    free(file);
    return ret;
}

ssize_t vfs_read(vfs_file_t* file, void* buf, size_t count) {
    if (!file || !file->ops || !file->ops->read) return -1;
    return file->ops->read(file->fh, buf, count);
}

ssize_t vfs_write(vfs_file_t* file, const void* buf, size_t count) {
    if (!file || !file->ops || !file->ops->write) return -1;
    return file->ops->write(file->fh, buf, count);
}

off_t vfs_seek(vfs_file_t* file, off_t offset, vfs_whence_t whence) {
    if (!file || !file->ops || !file->ops->seek) return -1;
    return file->ops->seek(file->fh, offset, whence);
}

off_t vfs_tell(vfs_file_t* file) {
    if (!file || !file->ops || !file->ops->tell) return -1;
    return file->ops->tell(file->fh);
}

int vfs_stat(const char* path, vfs_stat_t* st) {
    if (!path || !st) return -1;
    const char* rel_path;
    vfs_mount_t* mount = vfs_find_mount(path, &rel_path);
    if (!mount || !mount->ops->stat) return -1;
    return mount->ops->stat(rel_path, st);
}

int vfs_unlink(const char* path) {
    if (!path) return -1;
    const char* rel_path;
    vfs_mount_t* mount = vfs_find_mount(path, &rel_path);
    if (!mount || !mount->ops->unlink) return -1;
    return mount->ops->unlink(rel_path);
}

int vfs_rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath) return -1;
    const char* rel_old;
    vfs_mount_t* mount = vfs_find_mount(oldpath, &rel_old);
    if (!mount || !mount->ops->rename) return -1;
    
    const char* rel_new;
    vfs_mount_t* mount2 = vfs_find_mount(newpath, &rel_new);
    if (mount != mount2) return -1;
    
    return mount->ops->rename(rel_old, rel_new);
}

int vfs_mkdir(const char* path, uint16_t mode) {
    if (!path) return -1;
    const char* rel_path;
    vfs_mount_t* mount = vfs_find_mount(path, &rel_path);
    if (!mount || !mount->ops->mkdir) return -1;
    return mount->ops->mkdir(rel_path, mode);
}

int vfs_rmdir(const char* path) {
    if (!path) return -1;
    const char* rel_path;
    vfs_mount_t* mount = vfs_find_mount(path, &rel_path);
    if (!mount || !mount->ops->rmdir) return -1;
    return mount->ops->rmdir(rel_path);
}

vfs_dir_t* vfs_opendir(const char* path) {
    if (!path) return NULL;
    const char* rel_path;
    vfs_mount_t* mount = vfs_find_mount(path, &rel_path);
    if (!mount || !mount->ops->opendir) return NULL;
    
    vfs_dir_t* d = calloc(1, sizeof(vfs_dir_t));
    if (!d) return NULL;
    
    d->ops = mount->ops;
    d->dh = mount->ops->opendir(rel_path);
    if (!d->dh) {
        free(d);
        return NULL;
    }
    return d;
}

int vfs_readdir(vfs_dir_t* dir, vfs_dirent_t* entry) {
    if (!dir || !dir->ops || !dir->ops->readdir || !entry) return -1;
    return dir->ops->readdir(dir->dh, entry);
}

int vfs_closedir(vfs_dir_t* dir) {
    if (!dir || !dir->ops || !dir->ops->closedir) return -1;
    int ret = dir->ops->closedir(dir->dh);
    free(dir);
    return ret;
}

int vfs_sync(vfs_file_t* file) {
    if (!file || !file->ops || !file->ops->sync) return -1;
    return file->ops->sync(file->fh);
}

const char* vfs_get_mount_point(const char* path) {
    const char* rel_path;
    vfs_mount_t* mount = vfs_find_mount(path, &rel_path);
    return mount ? mount->prefix : NULL;
}