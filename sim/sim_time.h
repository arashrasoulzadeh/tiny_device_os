#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int sim_time_init(void);
void sim_time_cleanup(void);

uint64_t sim_time_now_us(void);
uint32_t sim_time_now_ms(void);

void sim_time_update(void);
void sim_time_sleep_ms(uint32_t ms);
void sim_time_sleep_us(uint32_t us);

#ifdef __cplusplus
}
#endif