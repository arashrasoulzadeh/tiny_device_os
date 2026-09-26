#include "driver.h"
#include "hal_uart.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct uart_device_data {
    hal_uart_t* hal_uart;
};

static int uart_probe(device_t* dev) {
    if (!dev) return -1;
    
    struct uart_device_data* data = calloc(1, sizeof(struct uart_device_data));
    if (!data) return -1;
    
    char bus_path[64];
    snprintf(bus_path, sizeof(bus_path), "/dev/uart0");
    
    hal_uart_config_t config = {
        .baudrate = 115200,
        .data_bits = 8,
        .parity = HAL_UART_PARITY_NONE,
        .stop_bits = HAL_UART_STOP_BITS_1,
        .flow_control = HAL_UART_FLOW_NONE,
        .rx_buffer_size = 256,
        .tx_buffer_size = 256
    };
    
    data->hal_uart = hal_uart_open(bus_path, &config);
    if (!data->hal_uart) {
        free(data);
        return -1;
    }
    
    dev->private_data = data;
    return 0;
}

static int uart_remove(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    
    struct uart_device_data* data = (struct uart_device_data*)dev->private_data;
    if (data->hal_uart) {
        hal_uart_close(data->hal_uart);
    }
    free(data);
    dev->private_data = NULL;
    return 0;
}

static int uart_open(device_t* dev, void** handle) {
    if (!dev || !dev->private_data) return -1;
    *handle = dev->private_data;
    return 0;
}

static int uart_close(void* handle) {
    (void)handle;
    return 0;
}

static ssize_t uart_read(void* handle, void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    struct uart_device_data* data = (struct uart_device_data*)handle;
    int ret = hal_uart_read(data->hal_uart, buf, count);
    return ret >= 0 ? ret : -1;
}

static ssize_t uart_write(void* handle, const void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    struct uart_device_data* data = (struct uart_device_data*)handle;
    int ret = hal_uart_write(data->hal_uart, buf, count);
    return ret >= 0 ? ret : -1;
}

static int uart_ioctl(void* handle, uint32_t cmd, void* arg) {
    if (!handle) return -1;
    
    struct uart_device_data* data = (struct uart_device_data*)handle;
    
    switch (cmd) {
        case 0x30:  // UART_SET_BAUD
            if (arg) {
                // Would need to reconfigure
                return 0;
            }
            break;
        case 0x31:  // UART_GET_BAUD
            if (arg) {
                // Return current baud
                return 0;
            }
            break;
        case 0x32:  // UART_SET_CONFIG
            if (arg) {
                // Would need to reconfigure
                return 0;
            }
            break;
        case 0x33:  // UART_GET_RX_AVAILABLE
            if (arg) {
                *(int*)arg = hal_uart_get_rx_available(data->hal_uart);
                return 0;
            }
            break;
        case 0x34:  // UART_GET_TX_FREE
            if (arg) {
                *(int*)arg = hal_uart_get_tx_free(data->hal_uart);
                return 0;
            }
            break;
        case 0x35:  // UART_FLUSH
            hal_uart_flush(data->hal_uart);
            return 0;
        case 0x36:  // UART_SET_BREAK
            if (arg) {
                hal_uart_break(data->hal_uart, *(bool*)arg);
                return 0;
            }
            break;
    }
    return -1;
}

static int uart_suspend(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct uart_device_data* data = (struct uart_device_data*)dev->private_data;
    return hal_uart_suspend(data->hal_uart);
}

static int uart_resume(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct uart_device_data* data = (struct uart_device_data*)dev->private_data;
    return hal_uart_resume(data->hal_uart);
}

static const driver_ops_t uart_ops = {
    .probe = uart_probe,
    .remove = uart_remove,
    .open = uart_open,
    .close = uart_close,
    .read = uart_read,
    .write = uart_write,
    .ioctl = uart_ioctl,
    .suspend = uart_suspend,
    .resume = uart_resume,
};

static driver_t g_uart_driver = {
    .name = "uart",
    .type = DRIVER_TYPE_CHAR,
    .ops = &uart_ops,
};

int uart_driver_init(void) {
    return driver_register(&g_uart_driver);
}

void uart_driver_deinit(void) {
    driver_unregister("uart");
}

int uart_create_device(const char* name) {
    device_t* dev = calloc(1, sizeof(device_t));
    if (!dev) return -1;
    
    if (name) {
        strncpy(dev->name, name, DEVICE_NAME_MAX - 1);
    } else {
        strncpy(dev->name, "uart0", DEVICE_NAME_MAX - 1);
    }
    
    strncpy(dev->path, "/dev/uart0", sizeof(dev->path) - 1);
    dev->driver = &g_uart_driver;
    
    return device_register(dev);
}