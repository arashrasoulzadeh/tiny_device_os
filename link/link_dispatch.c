#include "link_dispatch.h"

#include <string.h>

static int copy_str(const uint8_t *payload, size_t len, uint8_t tag, char *out,
                    size_t cap) {
  uint8_t raw[160];
  uint8_t n = 0;
  if (cap == 0) {
    return -1;
  }
  if (link_tlv_get(payload, len, tag, raw, sizeof(raw), &n) != 0) {
    out[0] = '\0';
    return -1;
  }
  if ((size_t)n >= cap) {
    n = (uint8_t)(cap - 1u);
  }
  memcpy(out, raw, n);
  out[n] = '\0';
  return 0;
}

static int put_error(uint8_t *reply, size_t cap, size_t *reply_len,
                     uint8_t *channel, uint16_t code, const char *text) {
  size_t used = 0;
  if (link_tlv_put_u16(reply, cap, &used, LINK_TAG_CODE, code) != 0 ||
      link_tlv_put_str(reply, cap, &used, LINK_TAG_ERRMSG, text) != 0) {
    return -1;
  }
  *reply_len = used;
  *channel = LINK_CH_ERROR;
  return 0;
}

static int put_ok(uint8_t *reply, size_t cap, size_t *reply_len,
                  uint8_t *channel) {
  size_t used = 0;
  if (link_tlv_put_u8(reply, cap, &used, LINK_TAG_STATUS, 0) != 0) {
    return -1;
  }
  *reply_len = used;
  *channel = LINK_CH_RESULT;
  return 0;
}

int link_dispatch_call(const link_handlers_t *handlers, const uint8_t *payload,
                       size_t len, uint8_t *reply, size_t cap,
                       size_t *reply_len, uint8_t *channel) {
  uint8_t method = 0;
  uint8_t n = 0;
  char app[32];
  char text[160];
  char body[160];
  int rc;
  if (!handlers || !reply || !reply_len || !channel || (!payload && len > 0)) {
    return -1;
  }
  if (link_tlv_get(payload, len, LINK_TAG_METHOD, &method, 1, &n) != 0 ||
      n != 1) {
    return put_error(reply, cap, reply_len, channel, LINK_ERR_BAD_ARGS,
                     "method");
  }
  if (method == LINK_METHOD_NOTIFY) {
    uint8_t extent = LINK_EXTENT_BAND;
    uint8_t dur_raw[4];
    uint32_t duration = 0;
    if (!handlers->notify ||
        copy_str(payload, len, LINK_TAG_TEXT, text, sizeof(text)) != 0 ||
        text[0] == '\0') {
      return put_error(reply, cap, reply_len, channel, LINK_ERR_BAD_ARGS,
                       "title");
    }
    if (copy_str(payload, len, LINK_TAG_BODY, body, sizeof(body)) != 0) {
      body[0] = '\0';
    }
    if (link_tlv_get(payload, len, LINK_TAG_EXTENT, &extent, 1, &n) != 0 ||
        n != 1) {
      extent = LINK_EXTENT_BAND;
    }
    if (link_tlv_get(payload, len, LINK_TAG_DURATION, dur_raw, 4, &n) == 0 &&
        n == 4) {
      duration = (uint32_t)dur_raw[0] | ((uint32_t)dur_raw[1] << 8) |
                 ((uint32_t)dur_raw[2] << 16) | ((uint32_t)dur_raw[3] << 24);
    }
    rc = handlers->notify(text, body[0] ? body : NULL, extent, duration,
                          handlers->user);
    if (rc != 0) {
      return put_error(reply, cap, reply_len, channel, LINK_ERR_BAD_ARGS,
                       "notify");
    }
    return put_ok(reply, cap, reply_len, channel);
  }
  if (method == LINK_METHOD_MESSAGE || method == LINK_METHOD_EVENT) {
    if (copy_str(payload, len, LINK_TAG_APP, app, sizeof(app)) != 0 ||
        app[0] == '\0' ||
        copy_str(payload, len, LINK_TAG_TEXT, text, sizeof(text)) != 0 ||
        text[0] == '\0') {
      return put_error(reply, cap, reply_len, channel, LINK_ERR_BAD_ARGS,
                       "app");
    }
    if (method == LINK_METHOD_MESSAGE) {
      if (!handlers->message) {
        return put_error(reply, cap, reply_len, channel,
                         LINK_ERR_UNKNOWN_METHOD, "message");
      }
      rc = handlers->message(app, text, handlers->user);
    } else {
      uint8_t extra[64];
      uint8_t extra_n = 0;
      if (!handlers->event) {
        return put_error(reply, cap, reply_len, channel,
                         LINK_ERR_UNKNOWN_METHOD, "event");
      }
      if (link_tlv_get(payload, len, LINK_TAG_BODY, extra, sizeof(extra),
                       &extra_n) != 0) {
        extra_n = 0;
      }
      rc = handlers->event(app, text, extra_n ? extra : NULL, extra_n,
                           handlers->user);
    }
    if (rc != 0) {
      uint16_t code = rc > 0 ? (uint16_t)rc : LINK_ERR_UNKNOWN_APP;
      return put_error(reply, cap, reply_len, channel, code, app);
    }
    return put_ok(reply, cap, reply_len, channel);
  }
  if (method == LINK_METHOD_KEY) {
    uint8_t pressed = 1;
    if (!handlers->key ||
        copy_str(payload, len, LINK_TAG_TEXT, text, sizeof(text)) != 0 ||
        text[0] == '\0') {
      return put_error(reply, cap, reply_len, channel, LINK_ERR_BAD_ARGS,
                       "key");
    }
    if (link_tlv_get(payload, len, LINK_TAG_PRESSED, &pressed, 1, &n) != 0 ||
        n != 1) {
      pressed = 1;
    }
    rc = handlers->key(text, pressed ? 1 : 0, handlers->user);
    if (rc != 0) {
      return put_error(reply, cap, reply_len, channel, LINK_ERR_BAD_ARGS,
                       "key");
    }
    return put_ok(reply, cap, reply_len, channel);
  }
  return put_error(reply, cap, reply_len, channel, LINK_ERR_UNKNOWN_METHOD,
                   "method");
}
