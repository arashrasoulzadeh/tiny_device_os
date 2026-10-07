#include "link_session.h"

#include "link_crypto.h"

#include <string.h>

void link_session_init(link_session_t *session, const uint8_t key[16],
                       uint8_t key_id, uint8_t tx_dir) {
  memset(session, 0, sizeof(*session));
  if (key) {
    memcpy(session->key, key, 16);
  }
  session->key_id = key_id;
  session->tx_dir = tx_dir;
}

static void nonce_for(const link_session_t *session, uint8_t direction,
                      uint64_t counter, uint8_t nonce[12]) {
  nonce[0] = direction;
  nonce[1] = session->key_id;
  nonce[2] = (uint8_t)(session->session & 0xffu);
  nonce[3] = (uint8_t)(session->session >> 8);
  nonce[4] = (uint8_t)(counter & 0xffu);
  nonce[5] = (uint8_t)((counter >> 8) & 0xffu);
  nonce[6] = (uint8_t)((counter >> 16) & 0xffu);
  nonce[7] = (uint8_t)((counter >> 24) & 0xffu);
  nonce[8] = (uint8_t)((counter >> 32) & 0xffu);
  nonce[9] = (uint8_t)((counter >> 40) & 0xffu);
  nonce[10] = (uint8_t)((counter >> 48) & 0xffu);
  nonce[11] = (uint8_t)((counter >> 56) & 0xffu);
}

int link_session_encode_hello(const link_session_t *session,
                              uint16_t session_id, uint16_t msg_id,
                              uint8_t *out, size_t cap) {
  link_frame_t frame;
  size_t used = 0;
  if (!session || !out) {
    return -1;
  }
  memset(&frame, 0, sizeof(frame));
  frame.version = LINK_VERSION;
  frame.channel = LINK_CH_HELLO;
  frame.msg_id = msg_id;
  if (link_tlv_put_u8(frame.payload, sizeof(frame.payload), &used,
                      LINK_TAG_VERSION, LINK_VERSION) != 0 ||
      link_tlv_put_u8(frame.payload, sizeof(frame.payload), &used,
                      LINK_TAG_KEY_ID, session->key_id) != 0 ||
      link_tlv_put_u16(frame.payload, sizeof(frame.payload), &used,
                       LINK_TAG_SESSION, session_id) != 0) {
    return -1;
  }
  frame.length = (uint16_t)used;
  return link_frame_encode(&frame, out, cap);
}

int link_session_accept_hello(link_session_t *session,
                              const link_frame_t *frame, uint16_t session_id) {
  uint8_t version = 0;
  uint8_t key_id = 0;
  uint8_t raw[2];
  uint8_t n = 0;
  uint16_t got_session;
  if (!session || !frame || frame->channel != LINK_CH_HELLO ||
      (frame->flags & LINK_FLAG_ENCRYPTED)) {
    return -1;
  }
  if (link_tlv_get(frame->payload, frame->length, LINK_TAG_VERSION, &version, 1,
                   &n) != 0 ||
      n != 1 || version != LINK_VERSION) {
    return -1;
  }
  if (link_tlv_get(frame->payload, frame->length, LINK_TAG_KEY_ID, &key_id, 1,
                   &n) != 0 ||
      n != 1 || key_id != session->key_id) {
    return -1;
  }
  if (link_tlv_get(frame->payload, frame->length, LINK_TAG_SESSION, raw, 2,
                   &n) != 0 ||
      n != 2) {
    return -1;
  }
  got_session = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
  if (session->tx_dir == LINK_DIR_DEVICE) {
    session->session = session_id;
  } else if (got_session == 0) {
    return -1;
  } else {
    session->session = got_session;
  }
  session->tx_counter = 0;
  session->rx_counter = 0;
  session->open = true;
  return 0;
}

static void header_aad(const link_frame_t *frame, uint8_t aad[7]) {
  aad[0] = frame->version ? frame->version : LINK_VERSION;
  aad[1] = frame->flags;
  aad[2] = frame->channel;
  aad[3] = (uint8_t)(frame->msg_id & 0xffu);
  aad[4] = (uint8_t)(frame->msg_id >> 8);
  aad[5] = (uint8_t)(frame->length & 0xffu);
  aad[6] = (uint8_t)(frame->length >> 8);
}

int link_session_seal(link_session_t *session, uint8_t channel, uint16_t msg_id,
                      const uint8_t *pt, size_t pt_len, uint8_t *out,
                      size_t cap) {
  link_frame_t frame;
  uint8_t nonce[12];
  uint8_t aad[15];
  uint64_t counter;
  if (!session || !session->open || !out || pt_len > LINK_MAX_PAYLOAD - 24u ||
      (pt_len > 0 && !pt)) {
    return -1;
  }
  counter = session->tx_counter + 1u;
  memset(&frame, 0, sizeof(frame));
  frame.version = LINK_VERSION;
  frame.flags = LINK_FLAG_ENCRYPTED;
  frame.channel = channel;
  frame.msg_id = msg_id;
  frame.length = (uint16_t)(8u + pt_len + 16u);
  frame.payload[0] = (uint8_t)(counter & 0xffu);
  frame.payload[1] = (uint8_t)((counter >> 8) & 0xffu);
  frame.payload[2] = (uint8_t)((counter >> 16) & 0xffu);
  frame.payload[3] = (uint8_t)((counter >> 24) & 0xffu);
  frame.payload[4] = (uint8_t)((counter >> 32) & 0xffu);
  frame.payload[5] = (uint8_t)((counter >> 40) & 0xffu);
  frame.payload[6] = (uint8_t)((counter >> 48) & 0xffu);
  frame.payload[7] = (uint8_t)((counter >> 56) & 0xffu);
  header_aad(&frame, aad);
  memcpy(aad + 7, frame.payload, 8);
  nonce_for(session, session->tx_dir, counter, nonce);
  if (link_aesgcm_encrypt(session->key, nonce, aad, sizeof(aad), pt, pt_len,
                          frame.payload + 8, frame.payload + 8 + pt_len) != 0) {
    return -1;
  }
  session->tx_counter = counter;
  return link_frame_encode(&frame, out, cap);
}

int link_session_open(link_session_t *session, const link_frame_t *frame,
                      link_frame_t *plain) {
  uint64_t counter;
  uint8_t nonce[12];
  uint8_t aad[15];
  size_t ct_len;
  uint8_t direction;
  if (!session || !session->open || !frame || !plain) {
    return -1;
  }
  if ((frame->flags & LINK_FLAG_ENCRYPTED) == 0 || frame->length < 24u) {
    return -1;
  }
  counter = (uint64_t)frame->payload[0] | ((uint64_t)frame->payload[1] << 8) |
            ((uint64_t)frame->payload[2] << 16) |
            ((uint64_t)frame->payload[3] << 24) |
            ((uint64_t)frame->payload[4] << 32) |
            ((uint64_t)frame->payload[5] << 40) |
            ((uint64_t)frame->payload[6] << 48) |
            ((uint64_t)frame->payload[7] << 56);
  if (counter == 0 || counter <= session->rx_counter) {
    return -1;
  }
  ct_len = (size_t)frame->length - 24u;
  direction =
      session->tx_dir == LINK_DIR_HOST ? LINK_DIR_DEVICE : LINK_DIR_HOST;
  nonce_for(session, direction, counter, nonce);
  header_aad(frame, aad);
  memcpy(aad + 7, frame->payload, 8);
  memset(plain, 0, sizeof(*plain));
  if (link_aesgcm_decrypt(session->key, nonce, aad, sizeof(aad),
                          frame->payload + 8, ct_len,
                          frame->payload + 8 + ct_len, plain->payload) != 0) {
    return -1;
  }
  session->rx_counter = counter;
  plain->version = frame->version;
  plain->flags = (uint8_t)(frame->flags & (uint8_t)~LINK_FLAG_ENCRYPTED);
  plain->channel = frame->channel;
  plain->msg_id = frame->msg_id;
  plain->length = (uint16_t)ct_len;
  return 0;
}
