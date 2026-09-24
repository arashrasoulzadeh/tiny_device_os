#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_UART_PARITY_NONE = 0,
    HAL_UART_PARITY_EVEN,
    HAL_UART_PARITY_ODD
} hal_uart_parity_t;

typedef enum {
    HAL_UART_STOP_BITS_1 = 1,
    HAL_UART_STOP_BITS_2 = 2
} hal_uart_stop_bits_t;

typedef enum {
    HAL_UART_FLOW_NONE = 0,
    HAL_UART_FLOW_RTS_CTS,
    HAL_UART_FLOW_XON_XOFF
} hal_uart_flow_control_t;

typedef struct hal_uart hal_uart_t;

typedef void (*hal_uart_rx_callback_t)(hal_uart_t* uart, void* arg);
typedef void (*hal_uart_tx_callback_t)(hal_uart_t* uart, void* arg);

typedef struct {
    uint32_t baudrate;
    uint8_t data_bits;
    hal_uart_parity_t parity;
    hal_uart_stop_bits_t stop_bits;
    hal_uart_flow_control_t flow_control;
    uint16_t rx_buffer_size;
    uint16_t tx_buffer_size;
} hal_uart_config_t;

hal_uart_t* hal_uart_open(const char* path, const hal_uart_config_t* config);
void hal_uart_close(hal_uart_t* uart);

int hal_uart_write(hal_uart_t* uart, const uint8_t* data, size_t len);
int hal_uart_read(hal_uart_t* uart, uint8_t* data, size_t len);
int hal_uart_read_byte(hal_uart_t* uart, uint8_t* byte);
int hal_uart_write_byte(hal_uart_t* uart, uint8_t byte);

int hal_uart_set_rx_callback(hal_uart_t* uart, hal_uart_rx_callback_t cb, void* arg);
int hal_uart_set_tx_callback(hal_uart_t* uart, hal_uart_tx_callback_t cb, void* arg);

int hal_uart_get_rx_available(const hal_uart_t* uart);
int hal_uart_get_tx_free(const hal_uart_t* uart);

void hal_uart_flush(hal_uart_t* uart);
void hal_uart_break(hal_uart_t* uart, bool enable);

const char* hal_uart_get_path(const hal_uart_t* uart);

#ifdef __cplusplus
}
#endif