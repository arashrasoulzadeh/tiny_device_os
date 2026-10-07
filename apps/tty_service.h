#pragma once

/**
 * USB shell. The host is a terminal. This service owns the line, the
 * commands, and keyboard attach. It does not draw on the panel.
 */

#include "link_dispatch.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*tty_cli_fn)(int argc, char** argv);

void tty_service_reset(void);
void tty_service_bind(const link_handlers_t* handlers);
void tty_service_set_reboot(void (*reboot)(void));

/** Register a text command for `run <name>`. The name must stay alive. */
int tty_cli_register(const char* name, tty_cli_fn fn);

/** Queue the first prompt. Call once after Hello is accepted. */
void tty_service_open(void);

/** Feed terminal bytes. Output is taken with tty_service_read(). */
void tty_service_input(const uint8_t* data, size_t n);

/** Refresh htop when a second has passed. */
void tty_service_poll(uint32_t now_ms);

/** Copy pending output and remove it. Returns the byte count. */
int tty_service_read(uint8_t* dst, size_t cap);

#ifdef __cplusplus
}
#endif
