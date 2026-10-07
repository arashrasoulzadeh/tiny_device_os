#include "link_bridge.h"

#include "app.h"
#include "ardubot_keys.h"
#include "fw/events.h"
#include "link_dispatch.h"
#include "link_session.h"
#include "notify_service.h"
#include "sim_gpio.h"
#include "tty_service.h"

#include <stdio.h>
#include <string.h>

#define LINK_MSG_SLOTS 4
#define LINK_MSG_APP 24
#define LINK_MSG_TEXT 96

typedef struct {
  char app[LINK_MSG_APP];
  char text[LINK_MSG_TEXT];
  int used;
} link_msg_t;

typedef struct {
  uint8_t *tx;
  size_t cap;
  size_t used;
} link_tx_t;

static link_session_t g_session;
static link_stream_t g_stream;
static link_msg_t g_msgs[LINK_MSG_SLOTS];
static uint16_t g_msg_id;

static int tx_append(link_tx_t *tx, const uint8_t *data, int n) {
  if (!tx || n < 0) {
    return -1;
  }
  if (tx->used + (size_t)n > tx->cap) {
    return -1;
  }
  if (n > 0) {
    memcpy(tx->tx + tx->used, data, (size_t)n);
    tx->used += (size_t)n;
  }
  return 0;
}

static int send_frame(link_tx_t *tx, const link_frame_t *frame) {
  uint8_t raw[LINK_MAX_FRAME];
  int n = link_frame_encode(frame, raw, sizeof(raw));
  return tx_append(tx, raw, n);
}

static int send_error(link_tx_t *tx, uint16_t code, const char *text) {
  link_frame_t frame;
  size_t used = 0;
  memset(&frame, 0, sizeof(frame));
  frame.version = LINK_VERSION;
  frame.flags = LINK_FLAG_ERROR;
  frame.channel = LINK_CH_ERROR;
  frame.msg_id = ++g_msg_id;
  if (link_tlv_put_u16(frame.payload, sizeof(frame.payload), &used,
                       LINK_TAG_CODE, code) != 0 ||
      link_tlv_put_str(frame.payload, sizeof(frame.payload), &used,
                       LINK_TAG_ERRMSG, text) != 0) {
    return -1;
  }
  frame.length = (uint16_t)used;
  return send_frame(tx, &frame);
}

static int on_notify(const char *title, const char *body, int extent,
                     uint32_t duration_ms, void *user) {
  notify_spec_t spec;
  (void)user;
  memset(&spec, 0, sizeof(spec));
  spec.title = title;
  spec.body = body;
  spec.extent =
      extent == LINK_EXTENT_FULL ? NOTIFY_EXTENT_FULL : NOTIFY_EXTENT_BAND;
  spec.duration_ms = duration_ms;
  return notify_post(&spec);
}

static int on_message(const char *app, const char *text, void *user) {
  int i;
  (void)user;
  if (!app_find(app)) {
    return LINK_ERR_UNKNOWN_APP;
  }
  for (i = 0; i < LINK_MSG_SLOTS; i++) {
    if (!g_msgs[i].used) {
      snprintf(g_msgs[i].app, sizeof(g_msgs[i].app), "%s", app);
      snprintf(g_msgs[i].text, sizeof(g_msgs[i].text), "%s", text);
      g_msgs[i].used = 1;
      return 0;
    }
  }
  return LINK_ERR_BUSY;
}

static int on_event(const char *app, const char *name, const uint8_t *payload,
                    size_t len, void *user) {
  char topic[EVENT_TOPIC_MAX];
  int wrote;
  uint32_t size;
  (void)user;
  if (!app_find(app)) {
    return LINK_ERR_UNKNOWN_APP;
  }
  wrote = snprintf(topic, sizeof(topic), "app.%s.%s", app, name);
  if (wrote < 0 || wrote >= (int)sizeof(topic)) {
    return LINK_ERR_BAD_ARGS;
  }
  size = (uint32_t)len;
  if (size > EVENT_MAX_DATA) {
    size = EVENT_MAX_DATA;
  }
  if (os_event_publish(topic, payload, size) < 0) {
    return LINK_ERR_BAD_ARGS;
  }
  return 0;
}

static sim_key_t key_from_name(const char *name) {
  if (strcmp(name, "up") == 0) {
    return SIM_KEY_UP;
  }
  if (strcmp(name, "down") == 0) {
    return SIM_KEY_DOWN;
  }
  if (strcmp(name, "left") == 0) {
    return SIM_KEY_LEFT;
  }
  if (strcmp(name, "right") == 0) {
    return SIM_KEY_RIGHT;
  }
  if (strcmp(name, "enter") == 0 || strcmp(name, "select") == 0) {
    return SIM_KEY_ENTER;
  }
  if (strcmp(name, "escape") == 0) {
    return SIM_KEY_ESCAPE;
  }
  return SIM_KEY_UNKNOWN;
}

static int on_key(const char *key, int pressed, void *user) {
  sim_key_t code;
  (void)user;
  code = key_from_name(key);
  if (code == SIM_KEY_UNKNOWN) {
    return -1;
  }
  sim_gpio_handle_key(code, pressed ? true : false);
  return 0;
}

static const link_handlers_t g_handlers = {
    on_notify, on_message, on_event, on_key, NULL,
};

static int seal_tty(link_tx_t *tx) {
  uint8_t chunk[200];
  while (tx->used < tx->cap && tx->cap - tx->used >= LINK_MAX_FRAME) {
    uint8_t sealed[LINK_MAX_FRAME];
    int n = tty_service_read(chunk, sizeof(chunk));
    int sn;
    if (n <= 0) {
      return 0;
    }
    sn = link_session_seal(&g_session, LINK_CH_TTY, ++g_msg_id, chunk,
                           (size_t)n, sealed, sizeof(sealed));
    if (sn < 0 || tx_append(tx, sealed, sn) != 0) {
      return -1;
    }
  }
  return 0;
}

static int on_wire(const uint8_t *raw, size_t len, void *user) {
  link_tx_t *tx = user;
  link_frame_t frame;
  link_frame_t plain;
  uint8_t reply[LINK_MAX_PAYLOAD];
  size_t reply_len = 0;
  uint8_t channel = 0;
  uint8_t sealed[LINK_MAX_FRAME];
  int n;
  if (link_frame_decode(raw, len, &frame) < 0) {
    return 0;
  }
  if (frame.channel == LINK_CH_HELLO &&
      (frame.flags & LINK_FLAG_ENCRYPTED) == 0) {
    if (link_session_accept_hello(&g_session, &frame, 1) != 0) {
      return send_error(tx, LINK_ERR_BAD_ARGS, "key");
    }
    n = link_session_encode_hello(&g_session, g_session.session, ++g_msg_id,
                                  sealed, sizeof(sealed));
    if (tx_append(tx, sealed, n) != 0) {
      return -1;
    }
    tty_service_open();
    return seal_tty(tx);
  }
  if (!g_session.open || link_session_open(&g_session, &frame, &plain) != 0) {
    return 0;
  }
  if (plain.channel == LINK_CH_TTY) {
    tty_service_input(plain.payload, plain.length);
    return seal_tty(tx);
  }
  if (plain.channel != LINK_CH_CALL) {
    return 0;
  }
  if (link_dispatch_call(&g_handlers, plain.payload, plain.length, reply,
                         sizeof(reply), &reply_len, &channel) != 0) {
    return 0;
  }
  n = link_session_seal(&g_session, channel, ++g_msg_id, reply, reply_len,
                        sealed, sizeof(sealed));
  return tx_append(tx, sealed, n);
}

void link_bridge_init(const uint8_t key[16], uint8_t key_id) {
  memset(&g_session, 0, sizeof(g_session));
  memset(&g_stream, 0, sizeof(g_stream));
  memset(g_msgs, 0, sizeof(g_msgs));
  g_msg_id = 0;
  link_session_init(&g_session, key, key_id, LINK_DIR_DEVICE);
  tty_service_reset();
  tty_service_bind(&g_handlers);
}

int link_bridge_rx(const uint8_t *data, size_t n, uint8_t *tx, size_t tx_cap) {
  link_tx_t out;
  if (!tx && tx_cap > 0) {
    return -1;
  }
  out.tx = tx;
  out.cap = tx_cap;
  out.used = 0;
  if (link_stream_push(&g_stream, data, n, on_wire, &out) != 0) {
    return -1;
  }
  return (int)out.used;
}

int link_bridge_poll(uint32_t now_ms, uint8_t *tx, size_t tx_cap) {
  link_tx_t out;
  if (!g_session.open || !tx) {
    return 0;
  }
  tty_service_poll(now_ms);
  out.tx = tx;
  out.cap = tx_cap;
  out.used = 0;
  if (seal_tty(&out) != 0) {
    return -1;
  }
  return (int)out.used;
}

int link_message_take(const char *app, char *buf, size_t cap) {
  int i;
  if (!app || !buf || cap == 0) {
    return -1;
  }
  for (i = 0; i < LINK_MSG_SLOTS; i++) {
    if (g_msgs[i].used && strcmp(g_msgs[i].app, app) == 0) {
      snprintf(buf, cap, "%s", g_msgs[i].text);
      g_msgs[i].used = 0;
      return 0;
    }
  }
  return -1;
}
