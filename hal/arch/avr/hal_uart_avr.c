#include "hal_uart.h"
#include "hal_power.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdlib.h>

#define UART_RX_BUFFER_SIZE 128
#define UART_TX_BUFFER_SIZE 128

typedef struct hal_uart {
    char path[32];
    hal_uart_config_t config;
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
    volatile uint16_t rx_head, rx_tail;
    volatile uint16_t tx_head, tx_tail;
    hal_uart_rx_callback_t rx_cb;
    void* rx_arg;
    hal_uart_tx_callback_t tx_cb;
    void* tx_arg;
    bool initialized;
} hal_uart_t;

static inline void uart_init_registers(hal_uart_t* uart) {
    uint16_t ubrr = F_CPU / 16 / uart->config.baudrate - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    
    uint8_t ucsrc = (1<<UCSZ01) | (1<<UCSZ00);
    
    if (uart->config.parity == HAL_UART_PARITY_EVEN) {
        UCSR0C |= (1<<UPM01);
    } else if (uart->config.parity == HAL_UART_PARITY_ODD) {
        UCSR0C |= (1<<UPM01) | (1<<UPM00);
    }
    
    if (uart->config.stop_bits == HAL_UART_STOP_BITS_2) {
        UCSR0C |= (1<<USBS0);
    }
    
    UCSR0C |= (1<<UCSZ01) | (1<<UCSZ00);
}

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
    
    uart->initialized = false;
    return uart;
}

void hal_uart_close(hal_uart_t* uart) {
    if (!uart) return;
    if (uart->initialized) {
        UCSR0B = 0;
    }
    free(uart);
}

int hal_uart_init(hal_uart_t* uart) {
    if (!uart || uart->initialized) return -1;
    
    uart_init_registers(uart);
    
    UCSR0B = (1<<RXEN0) | (1<<TXEN0);
    UCSR0B |= (1<<RXCIE0);
    
    sei();
    
    uart->initialized = true;
    return 0;
}

int hal_uart_write(hal_uart_t* uart, const uint8_t* data, size_t len) {
    if (!uart || !data || len == 0) return -1;
    if (!uart->initialized) return -1;
    
    size_t written = 0;
    while (written < len) {
        while (!(UCSR0A & (1<<UDRE0)));
        UDR0 = data[written++];
    }
    return written;
}

int hal_uart_read(hal_uart_t* uart, uint8_t* data, size_t len) {
    if (!uart || !data || len == 0) return -1;
    if (!uart->initialized) return -1;
    
    size_t read = 0;
    while (read < len) {
        while (uart->rx_head == uart->rx_tail);
        
        data[read++] = uart->rx_buffer[uart->rx_tail];
        uart->rx_tail = (uart->rx_tail + 1) % UART_RX_BUFFER_SIZE;
    }
    
    return read;
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

static void uart_init_registers(hal_uart_t* uart) {
    uint16_t ubrr = F_CPU / 16 / uart->config.baudrate - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    
    uint8_t ucsrc = (1<<UCSZ01) | (1<<UCSZ00);
    
    if (uart->config.parity == HAL_UART_PARITY_EVEN) {
        UCSR0C |= (1<<UPM01);
    } else if (uart->config.parity == HAL_UART_PARITY_ODD) {
        UCSR0C |= (1<<UPM01) | (1<<UPM00);
    }
    
    if (uart->config.stop_bits == HAL_UART_STOP_BITS_2) {
        UCSR0C |= (1<<USBS0);
    }
    
    UCSR0C |= (1<<UCSZ01) | (1<<UCSZ00);
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
    return 1;
}

void hal_uart_flush(hal_uart_t* uart) {
    if (!uart) return;
    while (!(UCSR0A & (1<<TXC0)));
}

void hal_uart_break(hal_uart_t* uart, bool enable) {
    (void)uart; (void)enable;
}

const char* hal_uart_get_path(const hal_uart_t* uart) {
    return uart ? uart->path : NULL;
}

int hal_uart_suspend(hal_uart_t* uart) {
    if (!uart) return -1;
    if (uart->initialized) {
        UCSR0B = 0;
        uart->initialized = false;
    }
    return 0;
}

int hal_uart_resume(hal_uart_t* uart) {
    if (!uart) return -1;
    if (!uart->initialized) {
        return hal_uart_init(uart);
    }
    return 0;
}