#pragma once

#include "link_frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_DIR_HOST 0u
#define LINK_DIR_DEVICE 1u

typedef struct {
  uint8_t key[16];
  uint8_t key_id;
  uint16_t session;
  uint64_t tx_counter;
  uint64_t rx_counter;
  uint8_t tx_dir;
  bool open;
} link_session_t;

void link_session_init(link_session_t *session, const uint8_t key[16],
                       uint8_t key_id, uint8_t tx_dir);

int link_session_encode_hello(const link_session_t *session,
                              uint16_t session_id, uint16_t msg_id,
                              uint8_t *out, size_t cap);

/** Device: adopt the host Hello and remember `session_id`. Host: adopt the
 * device Hello. */
int link_session_accept_hello(link_session_t *session,
                              const link_frame_t *frame, uint16_t session_id);

int link_session_seal(link_session_t *session, uint8_t channel, uint16_t msg_id,
                      const uint8_t *pt, size_t pt_len, uint8_t *out,
                      size_t cap);

/** Decrypts an encrypted frame into `plain` (payload is plaintext). Returns 0
 * or -1. */
int link_session_open(link_session_t *session, const link_frame_t *frame,
                      link_frame_t *plain);

#ifdef __cplusplus
}
#endif
