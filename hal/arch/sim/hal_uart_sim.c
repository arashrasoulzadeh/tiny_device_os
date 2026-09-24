#include "hal_uart.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define UART_RX_BUFFER_SIZE 256
#define UART_TX_BUFFER_SIZE 256

struct hal_uart {
    char path[64];
    hal_uart_config_t config;
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
    size_t rx_head, rx_tail;
    size_t tx_head, tx_tail;
    hal_uart_rx_callback_t rx_cb;
    void* rx_arg;
    hal_uart_tx_callback_t tx_cb;
    void* tx_arg;
};

hal_uart_t* hal_uart_open(const char* path, const hal_uart_config_t* config) {
    hal_uart_t* uart = calloc(1, sizeof(hal_uart_t));
    if (!uart) return NULL;
    
    strncpy(uart->path, path, sizeof(uart->path) - 1);
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
    
    return uart;
}

void hal_uart_close(hal_uart_t* uart) {
    if (uart) free(uart);
}

int hal_uart_write(hal_uart_t* uart, const uint8_t* data, size_t len) {
    if (!uart || !data) return -1;
    
    size_t written = 0;
    while (written < len) {
        size_t next = (uart->tx_head + 1) % UART_TX_BUFFER_SIZE;
        if (next == uart->tx_tail) break;
        uart->tx_buffer[uart->tx_head] = data[written];
        uart->tx_head = next;
        written++;
    }
    
    if (uart->tx_cb && uart->tx_head != uart->tx_tail) {
        uart->tx_cb(uart, uart->tx_arg);
    }
    
    return (int)written;
}

int hal_uart_read(hal_uart_t* uart, uint8_t* data, size_t len) {
    if (!uart || !data) return -1;
    
    size_t read = 0;
    while (read < len && uart->rx_head != uart->rx_tail) {
        data[read] = uart->rx_buffer[uart->rx_tail];
        uart->rx_tail = (uart->rx_tail + 1) % UART_RX_BUFFER_SIZE;
        read++;
    }
    
    return (int)read;
}

int hal_uart_read_byte(hal_uart_t* uart, uint8_t* byte) {
    if (!uart || !byte || uart->rx_head == uart->rx_tail) return -1;
    *byte = uart->rx_buffer[uart->rx_tail];
    uart->rx_tail = (uart->rx_tail + 1) % UART_RX_BUFFER_SIZE;
    return 0;
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
    if (uart->rx_head >= uart->rx_tail) {
        return uart->rx_head - uart->rx_tail;
    }
    return UART_RX_BUFFER_SIZE - uart->rx_tail + uart->rx_head;
}

int hal_uart_get_tx_free(const hal_uart_t* uart) {
    if (!uart) return 0;
    if (uart->tx_tail > uart->tx_head) {
        return uart->tx_tail - uart->tx_head - 1;
    }
    return UART_TX_BUFFER_SIZE - uart->tx_head + uart->tx_tail - 1;
}

void hal_uart_flush(hal_uart_t* uart) {
    if (uart) {
        uart->tx_head = uart->tx_tail;
    }
}

void hal_uart_break(hal_uart_t* uart, bool enable) {
    (void)uart; (void)enable;
}

const char* hal_uart_get_path(const hal_uart_t* uart) {
    return uart ? uart->path : NULL;
}