#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run fn(arg) on a fresh stack ending at stack_top (exclusive / high address).
 * Does not return — fn must longjmp away. */
void host_call_on_stack(void* stack_top, void (*fn)(void*), void* arg);

/* Hosted sim/unit tests use large stacks; MCU targets cannot. */
#if defined(ARDUBOT_TARGET_ESP8266) || defined(ARDUBOT_TARGET_ESP32) || \
    defined(ARDUBOT_TARGET_AVR) || defined(ARDUBOT_TARGET_RP2040) || defined(ARDUBOT_PIO)
#ifndef HOST_STACK_MIN_BYTES
#define HOST_STACK_MIN_BYTES (2048u)
#endif
#else
#ifndef HOST_STACK_MIN_BYTES
#define HOST_STACK_MIN_BYTES (64u * 1024u)
#endif
#endif

#ifdef __cplusplus
}
#endif
