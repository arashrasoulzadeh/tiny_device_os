#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run fn(arg) on a fresh stack ending at stack_top (exclusive / high address).
 * Does not return — fn must longjmp away. */
void host_call_on_stack(void* stack_top, void (*fn)(void*), void* arg);

/* Minimum stack size for hosted (sim/unit-test) builds. */
#define HOST_STACK_MIN_BYTES (64u * 1024u)

#ifdef __cplusplus
}
#endif
