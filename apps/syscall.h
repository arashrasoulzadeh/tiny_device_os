#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSCALL_MAX_ARGS 6
#define CAPABILITY_MAX 32

typedef enum {
    SYS_EXIT = 0,
    SYS_YIELD,
    SYS_SLEEP,
    SYS_GET_TICKS,
    
    SYS_TASK_CREATE,
    SYS_TASK_DELETE,
    SYS_TASK_SUSPEND,
    SYS_TASK_RESUME,
    SYS_TASK_GET_CURRENT,
    SYS_TASK_GET_PRIORITY,
    SYS_TASK_SET_PRIORITY,
    
    SYS_MALLOC,
    SYS_FREE,
    SYS_REALLOC,
    SYS_GET_HEAP_INFO,
    
    SYS_GPIO_OPEN,
    SYS_GPIO_CLOSE,
    SYS_GPIO_READ,
    SYS_GPIO_WRITE,
    SYS_GPIO_TOGGLE,
    SYS_GPIO_SET_IRQ,
    
    SYS_I2C_OPEN,
    SYS_I2C_CLOSE,
    SYS_I2C_WRITE,
    SYS_I2C_READ,
    SYS_I2C_WRITE_READ,
    
    SYS_SPI_OPEN,
    SYS_SPI_CLOSE,
    SYS_SPI_TRANSFER,
    SYS_SPI_WRITE,
    SYS_SPI_READ,
    
    SYS_UART_OPEN,
    SYS_UART_CLOSE,
    SYS_UART_WRITE,
    SYS_UART_READ,
    
    SYS_DISPLAY_OPEN,
    SYS_DISPLAY_CLOSE,
    SYS_DISPLAY_INIT,
    SYS_DISPLAY_DRAW_BITMAP,
    SYS_DISPLAY_FILL_RECT,
    SYS_DISPLAY_SET_ROTATION,
    SYS_DISPLAY_SET_BRIGHTNESS,
    SYS_DISPLAY_SLEEP,
    SYS_DISPLAY_WAKE,
    SYS_DISPLAY_FLUSH,
    
    SYS_AUDIO_OPEN,
    SYS_AUDIO_CLOSE,
    SYS_AUDIO_START,
    SYS_AUDIO_STOP,
    SYS_AUDIO_WRITE,
    
    SYS_STORAGE_OPEN,
    SYS_STORAGE_CLOSE,
    SYS_STORAGE_READ,
    SYS_STORAGE_WRITE,
    SYS_STORAGE_ERASE,
    SYS_STORAGE_SYNC,
    SYS_STORAGE_GET_INFO,
    
    SYS_NET_OPEN,
    SYS_NET_CLOSE,
    SYS_NET_START,
    SYS_NET_STOP,
    SYS_NET_SEND,
    SYS_NET_RECV,
    
    SYS_WIFI_OPEN,
    SYS_WIFI_CLOSE,
    SYS_WIFI_CONNECT,
    SYS_WIFI_DISCONNECT,
    SYS_WIFI_SCAN,
    SYS_WIFI_GET_RSSI,
    
    SYS_FS_OPEN,
    SYS_FS_CLOSE,
    SYS_FS_READ,
    SYS_FS_WRITE,
    SYS_FS_SEEK,
    SYS_FS_TELL,
    SYS_FS_STAT,
    SYS_FS_UNLINK,
    SYS_FS_MKDIR,
    SYS_FS_RMDIR,
    SYS_FS_OPENDIR,
    SYS_FS_READDIR,
    SYS_FS_CLOSEDIR,
    
    SYS_CONFIG_OPEN,
    SYS_CONFIG_CLOSE,
    SYS_CONFIG_SET_STRING,
    SYS_CONFIG_GET_STRING,
    SYS_CONFIG_SET_INT,
    SYS_CONFIG_GET_INT,
    SYS_CONFIG_SET_BOOL,
    SYS_CONFIG_GET_BOOL,
    SYS_CONFIG_DELETE,
    SYS_CONFIG_FLUSH,
    
    SYS_EVENT_SUBSCRIBE,
    SYS_EVENT_UNSUBSCRIBE,
    SYS_EVENT_PUBLISH,
    
    SYS_POWER_GET_MODE,
    SYS_POWER_SET_CPU_FREQ,
    SYS_POWER_LIGHT_SLEEP,
    SYS_POWER_DEEP_SLEEP,
    SYS_POWER_ADD_GPIO_WAKE,
    SYS_POWER_ADD_RTC_WAKE,
    SYS_POWER_GET_WAKE_CAUSE,
    
    SYS_MODULE_LOAD,
    SYS_MODULE_UNLOAD,
    SYS_MODULE_GET_SYMBOL,
    
    SYS_APP_INSTALL,
    SYS_APP_START,
    SYS_APP_STOP,
    SYS_APP_UNINSTALL,
    SYS_APP_GET_INFO,
    
    SYS_MAX
} syscall_num_t;

typedef uint32_t capability_t;

typedef struct {
    capability_t caps[CAPABILITY_MAX / 32];
} capability_set_t;

#define CAP_GPIO_READ      (1u << 0)
#define CAP_GPIO_WRITE     (1u << 1)
#define CAP_I2C_ACCESS     (1u << 2)
#define CAP_SPI_ACCESS     (1u << 3)
#define CAP_UART_ACCESS    (1u << 4)
#define CAP_DISPLAY_ACCESS (1u << 5)
#define CAP_AUDIO_ACCESS   (1u << 6)
#define CAP_STORAGE_ACCESS (1u << 7)
#define CAP_NET_ACCESS     (1u << 8)
#define CAP_WIFI_ACCESS    (1u << 9)
#define CAP_FS_ACCESS      (1u << 10)
#define CAP_CONFIG_ACCESS  (1u << 11)
#define CAP_EVENT_ACCESS   (1u << 12)
#define CAP_POWER_MGMT     (1u << 13)
#define CAP_MODULE_LOAD    (1u << 14)
#define CAP_APP_MGMT       (1u << 15)

typedef int (*syscall_handler_t)(uint32_t* args, uint32_t* ret);

typedef struct {
    syscall_num_t num;
    syscall_handler_t handler;
    capability_t required_cap;
    const char* name;
} syscall_entry_t;

int syscall_init(void);
void syscall_deinit(void);

int syscall_register(const syscall_entry_t* entry);
int syscall_unregister(syscall_num_t num);

int syscall_invoke(syscall_num_t num, uint32_t* args, uint32_t* ret);

capability_set_t* capability_set_create(void);
void capability_set_destroy(capability_set_t* set);
void capability_set_add(capability_set_t* set, capability_t cap);
void capability_set_remove(capability_set_t* set, capability_t cap);
bool capability_set_has(const capability_set_t* set, capability_t cap);
void capability_set_clear(capability_set_t* set);
void capability_set_copy(capability_set_t* dst, const capability_set_t* src);

typedef struct app_context app_context_t;

app_context_t* app_context_create(uint32_t app_id, const capability_set_t* caps);
void app_context_destroy(app_context_t* ctx);
int app_context_check_cap(app_context_t* ctx, capability_t cap);

#ifdef __cplusplus
}
#endif