#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sim_net_callback_t)(const uint8_t* data, size_t len, void* arg);

int sim_net_init(void);
void sim_net_cleanup(void);

int sim_net_send(const uint8_t* data, size_t len);
void sim_net_set_callback(sim_net_callback_t cb, void* arg);

bool sim_net_is_connected(void);

#ifdef __cplusplus
}
#endif