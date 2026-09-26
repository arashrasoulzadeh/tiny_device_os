#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRIVER_NAME_MAX 32
#define DEVICE_NAME_MAX 32

typedef enum {
    DRIVER_TYPE_CHAR = 0,
    DRIVER_TYPE_BLOCK,
    DRIVER_TYPE_NETWORK,
    DRIVER_TYPE_DISPLAY,
    DRIVER_TYPE_SENSOR,
    DRIVER_TYPE_AUDIO,
    DRIVER_TYPE_BUS,
    DRIVER_TYPE_CUSTOM
} driver_type_t;

typedef struct driver driver_t;
typedef struct device device_t;
typedef struct driver_ops driver_ops_t;

struct driver_ops {
    int (*probe)(device_t* dev);
    int (*remove)(device_t* dev);
    int (*open)(device_t* dev, void** handle);
    int (*close)(void* handle);
    ssize_t (*read)(void* handle, void* buf, size_t count);
    ssize_t (*write)(void* handle, const void* buf, size_t count);
    int (*ioctl)(void* handle, uint32_t cmd, void* arg);
    int (*suspend)(device_t* dev);
    int (*resume)(device_t* dev);
};

struct driver {
    char name[DRIVER_NAME_MAX];
    driver_type_t type;
    const driver_ops_t* ops;
    void* private_data;
    int refcount;
    driver_t* next;
};

struct device {
    char name[DEVICE_NAME_MAX];
    char path[64];
    driver_t* driver;
    void* private_data;
    void* bus_data;
    uint32_t id;
    bool registered;
    device_t* next;
};

typedef struct {
    driver_t* drivers;
    device_t* devices;
    uint32_t next_device_id;
} driver_core_t;

int driver_core_init(void);
void driver_core_deinit(void);

int driver_register(driver_t* driver);
int driver_unregister(const char* name);
driver_t* driver_find(const char* name);

int device_register(device_t* dev);
int device_unregister(const char* name);
device_t* device_find(const char* name);
device_t* device_find_by_path(const char* path);

int device_open(const char* path, void** handle);
int device_close(void* handle);
ssize_t device_read(void* handle, void* buf, size_t count);
ssize_t device_write(void* handle, const void* buf, size_t count);
int device_ioctl(void* handle, uint32_t cmd, void* arg);

int device_suspend(const char* path);
int device_resume(const char* path);

void driver_core_lock(void);
void driver_core_unlock(void);

#ifdef __cplusplus
}
#endif