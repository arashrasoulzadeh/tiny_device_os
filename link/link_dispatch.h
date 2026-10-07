#pragma once

#include "link_frame.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int (*notify)(const char *title, const char *body, int extent,
                uint32_t duration_ms, void *user);
  int (*message)(const char *app, const char *text, void *user);
  int (*event)(const char *app, const char *name, const uint8_t *payload,
               size_t len, void *user);
  int (*key)(const char *key, int pressed, void *user);
  void *user;
} link_handlers_t;

/**
 * Reads one call payload. Writes an error-channel payload (code + message) when
 * the call is rejected, or a one-byte status result when it is accepted.
 * *channel is LINK_CH_RESULT or LINK_CH_ERROR.
 * Returns 0, or -1 if the reply does not fit.
 */
int link_dispatch_call(const link_handlers_t *handlers, const uint8_t *payload,
                       size_t len, uint8_t *reply, size_t cap,
                       size_t *reply_len, uint8_t *channel);

#ifdef __cplusplus
}
#endif
