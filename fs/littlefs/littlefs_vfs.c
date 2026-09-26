#include "littlefs_vfs.h"
#include "lfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OPEN_FILES 8
#define MAX_OPEN_DIRS 4

typedef struct {
  lfs_file_t file;
  bool in_use;
  char path[VFS_PATH_MAX];
} lfs_file_handle_t;

typedef struct {
  lfs_dir_t dir;
  bool in_use;
} lfs_dir_handle_t;

static lfs_file_handle_t g_files[MAX_OPEN_FILES];
static lfs_dir_handle_t g_dirs[MAX_OPEN_DIRS];
static lfs_fs_ctx_t *g_ctx = NULL;

static lfs_file_handle_t *alloc_file(void) {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!g_files[i].in_use) {
      g_files[i].in_use = true;
      return &g_files[i];
    }
  }
  return NULL;
}

static void free_file(lfs_file_handle_t *fh) {
  if (fh) {
    fh->in_use = false;
    fh->path[0] = '\0';
  }
}

static lfs_dir_handle_t *alloc_dir(void) {
  for (int i = 0; i < MAX_OPEN_DIRS; i++) {
    if (!g_dirs[i].in_use) {
      g_dirs[i].in_use = true;
      return &g_dirs[i];
    }
  }
  return NULL;
}

static void free_dir(lfs_dir_handle_t *dh) {
  if (dh)
    dh->in_use = false;
}

static int lfs_read(const struct lfs_config *c, lfs_block_t block,
                    lfs_off_t off, void *buffer, lfs_size_t size) {
  (void)c;
  hal_storage_t *storage = g_ctx->storage;
  return hal_storage_read(
      storage, g_ctx->offset + block * g_ctx->block_size + off, buffer, size);
}

static int lfs_prog(const struct lfs_config *c, lfs_block_t block,
                    lfs_off_t off, const void *buffer, lfs_size_t size) {
  (void)c;
  hal_storage_t *storage = g_ctx->storage;
  return hal_storage_write(
      storage, g_ctx->offset + block * g_ctx->block_size + off, buffer, size);
}

static int lfs_erase(const struct lfs_config *c, lfs_block_t block) {
  (void)c;
  hal_storage_t *storage = g_ctx->storage;
  return hal_storage_erase(storage, g_ctx->offset + block * g_ctx->block_size,
                           g_ctx->block_size);
}

static int lfs_sync(const struct lfs_config *c) {
  (void)c;
  hal_storage_t *storage = g_ctx->storage;
  return hal_storage_sync(storage);
}

static int littlefs_open(const char *path, vfs_mode_t mode, vfs_file_t **file) {
  if (!g_ctx || !path || !file)
    return -1;

  lfs_file_handle_t *fh = alloc_file();
  if (!fh)
    return -1;

  int flags = 0;
  if (mode & VFS_MODE_READ)
    flags |= LFS_O_RDONLY;
  if (mode & VFS_MODE_WRITE)
    flags |= LFS_O_WRONLY;
  if (mode & VFS_MODE_APPEND)
    flags |= LFS_O_APPEND;
  if (mode & VFS_MODE_CREATE)
    flags |= LFS_O_CREAT;
  if (mode & VFS_MODE_TRUNC)
    flags |= LFS_O_TRUNC;
  if (mode & VFS_MODE_EXCL)
    flags |= LFS_O_EXCL;

  int ret = lfs_file_open(&g_ctx->lfs, &fh->file, path, flags);
  if (ret < 0) {
    free_file(fh);
    return ret;
  }

  strncpy(fh->path, path, VFS_PATH_MAX - 1);

  vfs_file_t *vf = calloc(1, sizeof(vfs_file_t));
  if (!vf) {
    lfs_file_close(&g_ctx->lfs, &fh->file);
    free_file(fh);
    return -1;
  }

  vf->fh = fh;
  *file = vf;
  return 0;
}

static int littlefs_close(vfs_file_t *file) {
  if (!file || !file->fh)
    return -1;

  lfs_file_handle_t *fh = (lfs_file_handle_t *)file->fh;
  int ret = lfs_file_close(&g_ctx->lfs, &fh->file);
  free_file(fh);
  free(file);
  return ret;
}

static ssize_t littlefs_read(vfs_file_t *file, void *buf, size_t count) {
  if (!file || !file->fh || !buf)
    return -1;

  lfs_file_handle_t *fh = (lfs_file_handle_t *)file->fh;
  lfs_ssize_t ret = lfs_file_read(&g_ctx->lfs, &fh->file, buf, count);
  return ret < 0 ? -1 : (ssize_t)ret;
}

static ssize_t littlefs_write(vfs_file_t *file, const void *buf, size_t count) {
  if (!file || !file->fh || !buf)
    return -1;

  lfs_file_handle_t *fh = (lfs_file_handle_t *)file->fh;
  lfs_ssize_t ret = lfs_file_write(&g_ctx->lfs, &fh->file, buf, count);
  return ret < 0 ? -1 : (ssize_t)ret;
}

static off_t littlefs_seek(vfs_file_t *file, off_t offset,
                           vfs_whence_t whence) {
  if (!file || !file->fh)
    return -1;

  lfs_file_handle_t *fh = (lfs_file_handle_t *)file->fh;
  int lfs_whence;
  switch (whence) {
  case VFS_SEEK_SET:
    lfs_whence = LFS_SEEK_SET;
    break;
  case VFS_SEEK_CUR:
    lfs_whence = LFS_SEEK_CUR;
    break;
  case VFS_SEEK_END:
    lfs_whence = LFS_SEEK_END;
    break;
  default:
    return -1;
  }

  lfs_soff_t ret = lfs_file_seek(&g_ctx->lfs, &fh->file, offset, lfs_whence);
  return ret < 0 ? -1 : (off_t)ret;
}

static off_t littlefs_tell(vfs_file_t *file) {
  if (!file || !file->fh)
    return -1;

  lfs_file_handle_t *fh = (lfs_file_handle_t *)file->fh;
  lfs_soff_t ret = lfs_file_tell(&g_ctx->lfs, &fh->file);
  return ret < 0 ? -1 : (off_t)ret;
}

static int littlefs_stat(const char *path, vfs_stat_t *st) {
  if (!g_ctx || !path || !st)
    return -1;

  struct lfs_info info;
  int ret = lfs_stat(&g_ctx->lfs, path, &info);
  if (ret < 0)
    return ret;

  st->size = info.size;
  st->blocks = (info.size + g_ctx->block_size - 1) / g_ctx->block_size;
  st->mode = info.type == LFS_TYPE_DIR ? 040000 : 0100000;
  st->mtime = 0;
  st->ctime = 0;
  st->is_dir = (info.type == LFS_TYPE_DIR);

  return 0;
}

static int littlefs_unlink(const char *path) {
  if (!g_ctx || !path)
    return -1;
  return lfs_remove(&g_ctx->lfs, path);
}

static int littlefs_rename(const char *oldpath, const char *newpath) {
  if (!g_ctx || !oldpath || !newpath)
    return -1;
  return lfs_rename(&g_ctx->lfs, oldpath, newpath);
}

static int littlefs_mkdir(const char *path, uint16_t mode) {
  (void)mode;
  if (!g_ctx || !path)
    return -1;
  return lfs_mkdir(&g_ctx->lfs, path);
}

static int littlefs_rmdir(const char *path) {
  if (!g_ctx || !path)
    return -1;
  return lfs_remove(&g_ctx->lfs, path);
}

static vfs_dir_t *littlefs_opendir(const char *path) {
  if (!g_ctx || !path)
    return NULL;

  lfs_dir_handle_t *dh = alloc_dir();
  if (!dh)
    return NULL;

  int ret = lfs_dir_open(&g_ctx->lfs, &dh->dir, path);
  if (ret < 0) {
    free_dir(dh);
    return NULL;
  }

  vfs_dir_t *vd = calloc(1, sizeof(vfs_dir_t));
  if (!vd) {
    lfs_dir_close(&g_ctx->lfs, &dh->dir);
    free_dir(dh);
    return NULL;
  }

  vd->dh = dh;
  return vd;
}

static int littlefs_readdir(vfs_dir_t *dir, vfs_dirent_t *entry) {
  if (!dir || !dir->dh || !entry)
    return -1;

  lfs_dir_handle_t *dh = (lfs_dir_handle_t *)dir->dh;
  struct lfs_info info;
  int ret = lfs_dir_read(&g_ctx->lfs, &dh->dir, &info);
  if (ret <= 0)
    return ret == 0 ? 1 : -1;

  strncpy(entry->name, info.name, VFS_NAME_MAX - 1);
  entry->size = info.size;
  entry->is_dir = (info.type == LFS_TYPE_DIR);

  return 0;
}

static int littlefs_closedir(vfs_dir_t *dir) {
  if (!dir || !dir->dh)
    return -1;

  lfs_dir_handle_t *dh = (lfs_dir_handle_t *)dir->dh;
  int ret = lfs_dir_close(&g_ctx->lfs, &dh->dir);
  free_dir(dh);
  free(dir);
  return ret;
}

static int littlefs_sync(vfs_file_t *file) {
  if (!file || !file->fh)
    return -1;

  lfs_file_handle_t *fh = (lfs_file_handle_t *)file->fh;
  return lfs_file_sync(&g_ctx->lfs, &fh->file);
}

static const vfs_ops_t g_littlefs_ops = {
    .open = littlefs_open,
    .close = littlefs_close,
    .read = littlefs_read,
    .write = littlefs_write,
    .seek = littlefs_seek,
    .tell = littlefs_tell,
    .stat = littlefs_stat,
    .unlink = littlefs_unlink,
    .rename = littlefs_rename,
    .mkdir = littlefs_mkdir,
    .rmdir = littlefs_rmdir,
    .opendir = littlefs_opendir,
    .readdir = littlefs_readdir,
    .closedir = littlefs_closedir,
    .sync = littlefs_sync,
};

const vfs_ops_t *littlefs_get_ops(void) { return &g_littlefs_ops; }

int littlefs_mount(hal_storage_t *storage, uint32_t offset, uint32_t size,
                   uint32_t block_size, const char *mount_point) {
  if (!storage || !mount_point)
    return -1;

  g_ctx = calloc(1, sizeof(lfs_fs_ctx_t));
  if (!g_ctx)
    return -1;

  g_ctx->storage = storage;
  g_ctx->offset = offset;
  g_ctx->size = size;
  g_ctx->block_size = block_size;
  g_ctx->read_size = 16;
  g_ctx->prog_size = 16;
  g_ctx->lookahead_size = 16;

  g_ctx->lfs_cfg = calloc(1, sizeof(struct lfs_config));
  if (!g_ctx->lfs_cfg) {
    free(g_ctx);
    g_ctx = NULL;
    return -1;
  }

  g_ctx->lfs_cfg->read = lfs_read;
  g_ctx->lfs_cfg->prog = lfs_prog;
  g_ctx->lfs_cfg->erase = lfs_erase;
  g_ctx->lfs_cfg->sync = lfs_sync;
  g_ctx->lfs_cfg->read_size = g_ctx->read_size;
  g_ctx->lfs_cfg->prog_size = g_ctx->prog_size;
  g_ctx->lfs_cfg->block_size = g_ctx->block_size;
  g_ctx->lfs_cfg->block_count = size / block_size;
  g_ctx->lfs_cfg->lookahead_size = g_ctx->lookahead_size;
  g_ctx->lfs_cfg->cache_size = 16;
  g_ctx->lfs_cfg->block_cycles = -1;

  int ret = lfs_mount(&g_ctx->lfs, g_ctx->lfs_cfg);
  if (ret == LFS_ERR_CORRUPT) {
    ret = lfs_format(&g_ctx->lfs, g_ctx->lfs_cfg);
    if (ret == 0) {
      ret = lfs_mount(&g_ctx->lfs, g_ctx->lfs_cfg);
    }
  }

  if (ret < 0) {
    free(g_ctx->lfs_cfg);
    free(g_ctx);
    g_ctx = NULL;
    return ret;
  }

  return vfs_mount(mount_point, &g_littlefs_ops, g_ctx);
}

int littlefs_unmount(const char *mount_point) {
  if (!g_ctx || !mount_point)
    return -1;

  int ret = vfs_unmount(mount_point);
  if (ret < 0)
    return ret;

  lfs_unmount(&g_ctx->lfs);
  free(g_ctx->lfs_cfg);
  free(g_ctx);
  g_ctx = NULL;

  return 0;
}

int littlefs_format(hal_storage_t *storage, uint32_t offset, uint32_t size,
                    uint32_t block_size) {
  if (!storage)
    return -1;

  struct lfs_config cfg = {
      .read = lfs_read,
      .prog = lfs_prog,
      .erase = lfs_erase,
      .sync = lfs_sync,
      .read_size = 16,
      .prog_size = 16,
      .block_size = block_size,
      .block_count = size / block_size,
      .lookahead_size = 16,
      .cache_size = 16,
      .block_cycles = -1,
  };

  g_ctx = calloc(1, sizeof(lfs_fs_ctx_t));
  if (!g_ctx)
    return -1;
  g_ctx->storage = storage;
  g_ctx->offset = offset;
  g_ctx->size = size;
  g_ctx->block_size = block_size;

  lfs_t lfs;
  int ret = lfs_format(&lfs, &cfg);

  free(g_ctx);
  g_ctx = NULL;

  return ret;
}

int littlefs_get_info(const char *mount_point, uint32_t *total,
                      uint32_t *used) {
  (void)mount_point;
  if (!g_ctx || !total || !used)
    return -1;

  lfs_soff_t total_bytes = lfs_fs_size(&g_ctx->lfs);
  if (total_bytes < 0)
    return -1;

  *total = g_ctx->size;
  *used = (uint32_t)total_bytes;

  return 0;
}