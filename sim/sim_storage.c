#define _POSIX_C_SOURCE 200809L
#include "sim_storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#define os_open _open
#define os_close _close
#define os_read _read
#define os_write _write
#define os_lseek _lseek
#define os_fstat _fstat
static int os_ftruncate(int fd, long size) { return _chsize(fd, size); }
#else
#include <unistd.h>
#define os_open open
#define os_close close
#define os_read read
#define os_write write
#define os_lseek lseek
#define os_fstat fstat
#define os_ftruncate ftruncate
#endif

#define FLASH_SIZE (4 * 1024 * 1024)
#define SD_SIZE (32 * 1024 * 1024)

static int g_flash_fd = -1;
static int g_sd_fd = -1;
static char g_flash_path[256] = "flash.img";
static char g_sd_path[256] = "sd.img";

static int create_image_file(const char* path, uint32_t size) {
#ifdef _WIN32
    int fd = os_open(path, _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    int fd = os_open(path, O_RDWR | O_CREAT, 0644);
#endif
    if (fd < 0) return -1;
    
    struct stat st;
    if (os_fstat(fd, &st) == 0) {
        if ((uint32_t)st.st_size >= size) {
            return fd;
        }
    }
    
    if (os_ftruncate(fd, (long)size) < 0) {
        os_close(fd);
        return -1;
    }
    
    return fd;
}

int sim_storage_init(const char* flash_image, const char* sd_image) {
    if (flash_image) strncpy(g_flash_path, flash_image, sizeof(g_flash_path) - 1);
    if (sd_image) strncpy(g_sd_path, sd_image, sizeof(g_sd_path) - 1);
    
    g_flash_fd = create_image_file(g_flash_path, FLASH_SIZE);
    if (g_flash_fd < 0) {
        perror("Failed to create flash image");
        return -1;
    }
    
    g_sd_fd = create_image_file(g_sd_path, SD_SIZE);
    if (g_sd_fd < 0) {
        perror("Failed to create SD image");
        os_close(g_flash_fd);
        g_flash_fd = -1;
        return -1;
    }
    
    return 0;
}

void sim_storage_cleanup(void) {
    if (g_flash_fd >= 0) {
        os_close(g_flash_fd);
        g_flash_fd = -1;
    }
    if (g_sd_fd >= 0) {
        os_close(g_sd_fd);
        g_sd_fd = -1;
    }
}

#define FLASH_SIZE (4 * 1024 * 1024)
#define SD_SIZE (32 * 1024 * 1024)

static int do_read(int fd, uint32_t offset, void* buffer, size_t size, uint32_t max_size) {
    if (fd < 0 || !buffer) return -1;
    if (offset + size > max_size) return -1;
    if (os_lseek(fd, (long)offset, SEEK_SET) < 0) return -1;
    return os_read(fd, buffer, (unsigned)size) == (int)size ? 0 : -1;
}

static int do_write(int fd, uint32_t offset, const void* buffer, size_t size, uint32_t max_size) {
    if (fd < 0 || !buffer) return -1;
    if (offset + size > max_size) return -1;
    if (os_lseek(fd, (long)offset, SEEK_SET) < 0) return -1;
    return os_write(fd, buffer, (unsigned)size) == (int)size ? 0 : -1;
}

int sim_storage_flash_read(uint32_t offset, void* buffer, size_t size) {
    return do_read(g_flash_fd, offset, buffer, size, FLASH_SIZE);
}

int sim_storage_flash_write(uint32_t offset, const void* buffer, size_t size) {
    return do_write(g_flash_fd, offset, buffer, size, FLASH_SIZE);
}

int sim_storage_flash_erase(uint32_t offset, size_t size) {
    if (g_flash_fd < 0) return -1;
    if (offset + size > FLASH_SIZE) return -1;
    uint8_t* buf = malloc(size);
    if (!buf) return -1;
    memset(buf, 0xFF, size);
    int ret = do_write(g_flash_fd, offset, buf, size, FLASH_SIZE);
    free(buf);
    return ret;
}

int sim_storage_sd_read(uint32_t offset, void* buffer, size_t size) {
    return do_read(g_sd_fd, offset, buffer, size, SD_SIZE);
}

int sim_storage_sd_write(uint32_t offset, const void* buffer, size_t size) {
    return do_write(g_sd_fd, offset, buffer, size, SD_SIZE);
}

bool sim_storage_flash_exists(void) {
    return g_flash_fd >= 0;
}

bool sim_storage_sd_exists(void) {
    return g_sd_fd >= 0;
}

uint32_t sim_storage_flash_size(void) {
    return FLASH_SIZE;
}

uint32_t sim_storage_sd_size(void) {
    return SD_SIZE;
}