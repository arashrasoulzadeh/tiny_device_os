#include "hal_uart.h"
#include "hal_power.h"
#include <driver/uart.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>

#define UART_RX_BUFFER_SIZE 1024
#define UART_TX_BUFFER_SIZE 1024

typedef struct hal_uart {
    char path[32];
    uart_port_t port;
    hal_uart_config_t config;
    uint8_t* rx_buffer;
    uint8_t* tx_buffer;
    hal_uart_rx_callback_t rx_cb;
    void* rx_arg;
    hal_uart_tx_callback_t tx_cb;
    void* tx_arg;
    bool initialized;
} hal_uart_t;

static uart_port_t hal_to_esp_port(const char* path) {
    if (strstr(path, "uart1") || strstr(path, "uart_1") || strstr(path, "uart2")) {
        return UART_NUM_1;
    } else if (strstr(path, "uart2") || strstr(path, "uart_2") || strstr(path, "uart3")) {
        return UART_NUM_2;
    }
    return UART_NUM_0;
}

static uart_parity_t hal_to_esp_parity(hal_uart_parity_t parity) {
    switch (parity) {
        case HAL_UART_PARITY_EVEN: return UART_PARITY_EVEN;
        case HAL_UART_PARITY_ODD: return UART_PARITY_ODD;
        default: return UART_PARITY_DISABLE;
    }
}

static uart_stop_bits_t hal_to_esp_stop_bits(hal_uart_stop_bits_t stop_bits) {
    switch (stop_bits) {
        case HAL_UART_STOP_BITS_2: return UART_STOP_BITS_2;
        default: return UART_STOP_BITS_1;
    }
}

static uart_hw_flowcontrol_t hal_to_esp_flow(hal_uart_flow_control_t flow) {
    switch (flow) {
        case HAL_UART_FLOW_RTS_CTS: return UART_HW_FLOWCTRL_RTS | UART_HW_FLOWCTRL_CTS;
        case HAL_UART_FLOW_XON_XOFF: return UART_HW_FLOWCTRL_XON_XOFF;
        default: return UART_HW_FLOWCTRL_DISABLE;
    }
}

static void uart_isr_handler(void* arg) {
    hal_uart_t* uart = (hal_uart_t*)arg;
    uint8_t data;
    while (uart_read_bytes(uart->port, &data, 1, 0) > 0) {
        if (uart->rx_cb) {
            uart->rx_cb(uart, uart->rx_arg);
        }
    }
}

hal_uart_t* hal_uart_open(const char* path, const hal_uart_config_t* config) {
    hal_uart_t* uart = calloc(1, sizeof(hal_uart_t));
    if (!uart) return NULL;
    
    strncpy(uart->path, path, sizeof(uart->path) - 1);
    uart->port = hal_to_esp_port(path);
    
    if (config) {
        uart->config = *config;
    } else {
        uart->config.baudrate = 115200;
        uart->config.data_bits = 8;
        uart->config.parity = HAL_UART_PARITY_NONE;
        uart->config.stop_bits = HAL_UART_STOP_BITS_1;
        uart->config.flow_control = HAL_UART_FLOW_NONE;
        uart->config.rx_buffer_size = UART_RX_BUFFER_SIZE;
        uart->config.tx_buffer_size = UART_TX_BUFFER_SIZE;
    }
    
    uart->initialized = false;
    return uart;
}

void hal_uart_close(hal_uart_t* uart) {
    if (!uart) return;
    if (uart->initialized) {
        uart_driver_delete(uart->port);
    }
    free(uart);
}

int hal_uart_init(hal_uart_t* uart) {
    if (!uart || uart->initialized) return -1;
    
    uart_config_t uart_config = {
        .baud_rate = uart->config.baudrate,
        .data_bits = uart->config.data_bits,
        .parity = hal_to_esp_parity(uart->config.parity),
        .stop_bits = hal_to_esp_stop_bits(uart->config.stop_bits),
        .flow_ctrl = hal_to_esp_flow(uart->config.flow_control),
        .source_clk = UART_SCLK_APB,
    };
    
    esp_err_t err = uart_param_config(uart->port, &uart_config);
    if (err != ESP_OK) return -1;
    
    err = uart_driver_install(uart->port, uart->config.rx_buffer_size, 
                              uart->config.tx_buffer_size, 0, NULL, 0);
    if (err != ESP_OK) return -1;
    
    uart->initialized = true;
    return 0;
}

int hal_uart_write(hal_uart_t* uart, const uint8_t* data, size_t len) {
    if (!uart || !data || len == 0) return -1;
    if (!uart->initialized && hal_uart_init(uart) != 0) return -1;
    
    int written = uart_write_bytes(uart->port, (const char*)data, len);
    return written;
}

int hal_uart_read(hal_uart_t* uart, uint8_t* data, size_t len) {
    if (!uart || !data || len == 0) return -1;
    if (!uart->initialized && hal_uart_init(uart) != 0) return -1;
    
    int read = uart_read_bytes(uart->port, data, len, pdMS_TO_TICKS(100));
    return read;
}

int hal_uart_read_byte(hal_uart_t* uart, uint8_t* byte) {
    if (!uart || !byte) return -1;
    if (!uart->initialized && hal_uart_init(uart) != 0) return -1;
    
    int read = uart_read_bytes(uart->port, byte, 1, pdMS_TO_TICKS(100));
    return read == 1 ? 0 : -1;
}

int hal_uart_write_byte(hal_uart_t* uart, uint8_t byte) {
    return hal_uart_write(uart, &byte, 1);
}

int hal_uart_set_rx_callback(hal_uart_t* uart, hal_uart_rx_callback_t cb, void* arg) {
    if (!uart) return -1;
    uart->rx_cb = cb;
    uart->rx_arg = arg;
    return 0;
}

int hal_uart_set_tx_callback(hal_uart_t* uart, hal_uart_tx_callback_t cb, void* arg) {
    if (!uart) return -1;
    uart->tx_cb = cb;
    uart->tx_arg = arg;
    return 0;
}

int hal_uart_get_rx_available(const hal_uart_t* uart) {
    if (!uart) return 0;
    if (!uart->initialized) return 0;
    
    size_t size;
    uart_get_buffered_data_len(uart->port, &size);
    return size;
}

int hal_uart_get_tx_free(const hal_uart_t* uart) {
    if (!uart) return 0;
    if (!uart->initialized) return 0;
    
    uart_get_tx_buffer_free_size(uart->port);
    return 0;
}

void hal_uart_flush(hal_uart_t* uart) {
    if (!uart || !uart->initialized) return;
    uart_wait_tx_done(uart->port, pdMS_TO_TICKS(100));
}

void hal_uart_break(hal_uart_t* uart, bool enable) {
    if (!uart || !uart->initialized) return;
    uart_set_break(uart->port, enable);
}

const char* hal_uart_get_path(const hal_uart_t* uart) {
    return uart ? uart->path : NULL;
}