#include "driver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_DRIVERS 32
#define MAX_DEVICES 64

static driver_core_t g_driver_core = {0};
static int g_core_lock = 0;

static void driver_core_lock_impl(void) {
    while (__sync_lock_test_and_set(&g_core_lock, 1)) {
        // Spin
    }
}

static void driver_core_unlock_impl(void) {
    __sync_lock_release(&g_core_lock);
}

void driver_core_lock(void) {
    driver_core_lock_impl();
}

void driver_core_unlock(void) {
    driver_core_unlock_impl();
}

int driver_core_init(void) {
    g_driver_core.drivers = NULL;
    g_driver_core.devices = NULL;
    g_driver_core.next_device_id = 1;
    g_core_lock = 0;
    return 0;
}

void driver_core_deinit(void) {
    while (g_driver_core.devices) {
        device_t* dev = g_driver_core.devices;
        g_driver_core.devices = dev->next;
        if (dev->driver && dev->driver->ops && dev->driver->ops->remove) {
            dev->driver->ops->remove(dev);
        }
        free(dev);
    }
    while (g_driver_core.drivers) {
        driver_t* drv = g_driver_core.drivers;
        g_driver_core.drivers = drv->next;
        free(drv);
    }
}

int driver_register(driver_t* driver) {
    if (!driver || !driver->name[0] || !driver->ops) return -1;
    
    driver_core_lock();
    
    // Check if already registered
    for (driver_t* d = g_driver_core.drivers; d; d = d->next) {
        if (strcmp(d->name, driver->name) == 0) {
            driver_core_unlock();
            return -1;
        }
    }
    
    driver->refcount = 0;
    driver->next = g_driver_core.drivers;
    g_driver_core.drivers = driver;
    
    driver_core_unlock();
    return 0;
}

int driver_unregister(const char* name) {
    if (!name) return -1;
    
    driver_core_lock();
    
    driver_t** prev = &g_driver_core.drivers;
    for (driver_t* d = g_driver_core.drivers; d; d = d->next) {
        if (strcmp(d->name, name) == 0) {
            if (d->refcount > 0) {
                driver_core_unlock();
                return -1;
            }
            *prev = d->next;
            free(d);
            driver_core_unlock();
            return 0;
        }
        prev = &d->next;
    }
    
    driver_core_unlock();
    return -1;
}

driver_t* driver_find(const char* name) {
    if (!name) return NULL;
    
    driver_core_lock();
    
    for (driver_t* d = g_driver_core.drivers; d; d = d->next) {
        if (strcmp(d->name, name) == 0) {
            driver_core_unlock();
            return d;
        }
    }
    
    driver_core_unlock();
    return NULL;
}

int device_register(device_t* dev) {
    if (!dev || !dev->name[0] || !dev->driver) return -1;
    
    driver_core_lock();
    
    // Check if already registered
    for (device_t* d = g_driver_core.devices; d; d = d->next) {
        if (strcmp(d->name, dev->name) == 0) {
            driver_core_unlock();
            return -1;
        }
    }
    
    dev->id = g_driver_core.next_device_id++;
    dev->registered = true;
    dev->next = g_driver_core.devices;
    g_driver_core.devices = dev;
    
    // Increment driver refcount
    dev->driver->refcount++;
    
    // Call probe
    if (dev->driver->ops && dev->driver->ops->probe) {
        int ret = dev->driver->ops->probe(dev);
        if (ret != 0) {
            dev->driver->refcount--;
            g_driver_core.devices = dev->next;
            dev->registered = false;
            driver_core_unlock();
            return ret;
        }
    }
    
    driver_core_unlock();
    return 0;
}

int device_unregister(const char* name) {
    if (!name) return -1;
    
    driver_core_lock();
    
    device_t** prev = &g_driver_core.devices;
    for (device_t* d = g_driver_core.devices; d; d = d->next) {
        if (strcmp(d->name, name) == 0) {
            if (d->driver && d->driver->ops && d->driver->ops->remove) {
                d->driver->ops->remove(d);
            }
            if (d->driver) {
                d->driver->refcount--;
            }
            *prev = d->next;
            free(d);
            driver_core_unlock();
            return 0;
        }
        prev = &d->next;
    }
    
    driver_core_unlock();
    return -1;
}

device_t* device_find(const char* name) {
    if (!name) return NULL;
    
    driver_core_lock();
    
    for (device_t* d = g_driver_core.devices; d; d = d->next) {
        if (strcmp(d->name, name) == 0) {
            driver_core_unlock();
            return d;
        }
    }
    
    driver_core_unlock();
    return NULL;
}

device_t* device_find_by_path(const char* path) {
    if (!path) return NULL;
    
    driver_core_lock();
    
    for (device_t* d = g_driver_core.devices; d; d = d->next) {
        if (strcmp(d->path, path) == 0) {
            driver_core_unlock();
            return d;
        }
    }
    
    driver_core_unlock();
    return NULL;
}

static int device_open_internal(device_t* dev, void** handle) {
    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->open) {
        return -1;
    }
    return dev->driver->ops->open(dev, handle);
}

int device_open(const char* path, void** handle) {
    if (!path || !handle) return -1;
    
    driver_core_lock();
    device_t* dev = device_find_by_path(path);
    driver_core_unlock();
    
    if (!dev) return -1;
    
    return device_open_internal(dev, handle);
}

int device_close(void* handle) {
    if (!handle) return -1;
    
    // We need to find the device from the handle
    // This is a simplified implementation - in reality we'd track handles
    driver_core_lock();
    for (device_t* d = g_driver_core.devices; d; d = d->next) {
        if (d->driver && d->driver->ops && d->driver->ops->close) {
            int ret = d->driver->ops->close(handle);
            if (ret == 0) {
                driver_core_unlock();
                return 0;
            }
        }
    }
    driver_core_unlock();
    return -1;
}

ssize_t device_read(void* handle, void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    driver_core_lock();
    for (device_t* d = g_driver_core.devices; d; d = d->next) {
        if (d->driver && d->driver->ops && d->driver->ops->read) {
            ssize_t ret = d->driver->ops->read(handle, buf, count);
            if (ret >= 0) {
                driver_core_unlock();
                return ret;
            }
        }
    }
    driver_core_unlock();
    return -1;
}

ssize_t device_write(void* handle, const void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    driver_core_lock();
    for (device_t* d = g_driver_core.devices; d; d = d->next) {
        if (d->driver && d->driver->ops && d->driver->ops->write) {
            ssize_t ret = d->driver->ops->write(handle, buf, count);
            if (ret >= 0) {
                driver_core_unlock();
                return ret;
            }
        }
    }
    driver_core_unlock();
    return -1;
}

int device_ioctl(void* handle, uint32_t cmd, void* arg) {
    if (!handle) return -1;
    
    driver_core_lock();
    for (device_t* d = g_driver_core.devices; d; d = d->next) {
        if (d->driver && d->driver->ops && d->driver->ops->ioctl) {
            int ret = d->driver->ops->ioctl(handle, cmd, arg);
            if (ret >= 0) {
                driver_core_unlock();
                return ret;
            }
        }
    }
    driver_core_unlock();
    return -1;
}

int device_suspend(const char* path) {
    if (!path) return -1;
    
    driver_core_lock();
    device_t* dev = device_find_by_path(path);
    driver_core_unlock();
    
    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->suspend) {
        return -1;
    }
    return dev->driver->ops->suspend(dev);
}

int device_resume(const char* path) {
    if (!path) return -1;
    
    driver_core_lock();
    device_t* dev = device_find_by_path(path);
    driver_core_unlock();
    
    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->resume) {
        return -1;
    }
    return dev->driver->ops->resume(dev);
}