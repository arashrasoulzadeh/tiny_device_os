#pragma once

/**
 * Device side of the link session. Transport code feeds bytes to
 * link_bridge_rx() and writes the returned reply. The same calls work
 * for USB, and later for Bluetooth, Wi-Fi, or radio.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void link_bridge_init(const uint8_t key[16], uint8_t key_id);

/** Push received bytes. Returns how many reply bytes were written, or -1. */
int link_bridge_rx(const uint8_t *data, size_t n, uint8_t *tx, size_t tx_cap);

/**
 * Seal any shell output that became ready, including an htop refresh.
 * Returns reply bytes, or 0 when there is nothing to send.
 */
int link_bridge_poll(uint32_t now_ms, uint8_t *tx, size_t tx_cap);

/** Copy the next queued app.message for `app`. Returns 0, or -1 if none. */
int link_message_take(const char *app, char *buf, size_t cap);

#ifdef __cplusplus
}
#endif
