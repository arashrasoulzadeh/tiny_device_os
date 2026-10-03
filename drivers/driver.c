#include "driver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_DRIVERS 32
#define MAX_DEVICES 64

static driver_core_t g_driver_core = {0};
static int g_core_lock = 0;

/* device_close/read/write/ioctl used to have no way to tell which device
 * a handle belonged to, so each one just tried every registered device's
 * matching op in turn and used whichever one didn't return an error -
 * with two devices of the same driver type open at once (e.g. two GPIO
 * pins), a call with device B's handle could silently execute against
 * device A instead. This table is filled in by device_open() and makes
 * that dispatch exact instead of a guess. */
#define MAX_OPEN_HANDLES 32
static struct {
    void* handle;
    device_t* dev;
} g_open_handles[MAX_OPEN_HANDLES];

static void device_handle_track(void* handle, device_t* dev) {
    for (int i = 0; i < MAX_OPEN_HANDLES; i++) {
        if (!g_open_handles[i].handle) {
            g_open_handles[i].handle = handle;
            g_open_handles[i].dev = dev;
            return;
        }
    }
}

static device_t* device_handle_lookup(void* handle) {
    for (int i = 0; i < MAX_OPEN_HANDLES; i++) {
        if (g_open_handles[i].handle == handle) return g_open_handles[i].dev;
    }
    return NULL;
}

static void device_handle_untrack(void* handle) {
    for (int i = 0; i < MAX_OPEN_HANDLES; i++) {
        if (g_open_handles[i].handle == handle) {
            g_open_handles[i].handle = NULL;
            g_open_handles[i].dev = NULL;
            return;
        }
    }
}

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
    memset(g_open_handles, 0, sizeof(g_open_handles));
    return 0;
}

void driver_core_deinit(void) {
    /* driver_t/device_t are caller-owned (every real driver in drivers/
     * registers a static struct, e.g. gpio_driver.c's g_gpio_driver) -
     * this used to free() them anyway, which would corrupt the heap the
     * moment deinit ran with any real driver registered. Just unlink. */
    while (g_driver_core.devices) {
        device_t* dev = g_driver_core.devices;
        g_driver_core.devices = dev->next;
        if (dev->driver && dev->driver->ops && dev->driver->ops->remove) {
            dev->driver->ops->remove(dev);
        }
        dev->registered = false;
    }
    while (g_driver_core.drivers) {
        driver_t* drv = g_driver_core.drivers;
        g_driver_core.drivers = drv->next;
        drv->refcount = 0;
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
            /* d is caller-owned (a static struct, for every real driver
             * in this codebase) - free()ing it here used to corrupt the
             * heap on the very first unregister of a real driver. */
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
            d->registered = false;  // caller-owned - see driver_unregister()
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

    /* device_find_by_path() takes driver_core_lock() itself - taking it
     * again here before calling it deadlocked immediately, since this is
     * a plain spin lock with no reentrancy support. Same bug existed in
     * device_suspend()/device_resume() below. Never hit in practice only
     * because nothing anywhere calls driver_core_init() - this whole
     * driver/device core is disconnected from the real boot path, which
     * goes through the hal/sim layer instead. */
    device_t* dev = device_find_by_path(path);
    if (!dev) return -1;

    int ret = device_open_internal(dev, handle);
    if (ret == 0) device_handle_track(*handle, dev);
    return ret;
}

int device_close(void* handle) {
    if (!handle) return -1;

    driver_core_lock();
    device_t* dev = device_handle_lookup(handle);
    driver_core_unlock();

    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->close) return -1;

    int ret = dev->driver->ops->close(handle);
    if (ret == 0) {
        driver_core_lock();
        device_handle_untrack(handle);
        driver_core_unlock();
    }
    return ret;
}

ssize_t device_read(void* handle, void* buf, size_t count) {
    if (!handle || !buf) return -1;

    driver_core_lock();
    device_t* dev = device_handle_lookup(handle);
    driver_core_unlock();

    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->read) return -1;
    return dev->driver->ops->read(handle, buf, count);
}

ssize_t device_write(void* handle, const void* buf, size_t count) {
    if (!handle || !buf) return -1;

    driver_core_lock();
    device_t* dev = device_handle_lookup(handle);
    driver_core_unlock();

    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->write) return -1;
    return dev->driver->ops->write(handle, buf, count);
}

int device_ioctl(void* handle, uint32_t cmd, void* arg) {
    if (!handle) return -1;

    driver_core_lock();
    device_t* dev = device_handle_lookup(handle);
    driver_core_unlock();

    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->ioctl) return -1;
    return dev->driver->ops->ioctl(handle, cmd, arg);
}

int device_suspend(const char* path) {
    if (!path) return -1;

    device_t* dev = device_find_by_path(path);  // locks internally - see device_open()
    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->suspend) {
        return -1;
    }
    return dev->driver->ops->suspend(dev);
}

int device_resume(const char* path) {
    if (!path) return -1;

    device_t* dev = device_find_by_path(path);  // locks internally - see device_open()
    if (!dev || !dev->driver || !dev->driver->ops || !dev->driver->ops->resume) {
        return -1;
    }
    return dev->driver->ops->resume(dev);
}