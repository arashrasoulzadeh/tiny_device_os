#include "link_frame.h"

#include <string.h>

uint16_t link_crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xffffu;
  size_t i;
  for (i = 0; i < len; i++) {
    int bit;
    crc ^= (uint16_t)data[i] << 8;
    for (bit = 0; bit < 8; bit++) {
      if (crc & 0x8000u) {
        crc = (uint16_t)((crc << 1) ^ 0x1021u);
      } else {
        crc = (uint16_t)(crc << 1);
      }
    }
  }
  return crc;
}

int link_frame_encode(const link_frame_t *frame, uint8_t *out, size_t cap) {
  size_t total;
  uint16_t crc;
  if (!frame || !out || frame->length > LINK_MAX_PAYLOAD) {
    return -1;
  }
  total = LINK_HEADER_LEN + frame->length + 2u;
  if (cap < total) {
    return -1;
  }
  out[0] = (uint8_t)(LINK_MAGIC & 0xffu);
  out[1] = (uint8_t)(LINK_MAGIC >> 8);
  out[2] = frame->version ? frame->version : LINK_VERSION;
  out[3] = frame->flags;
  out[4] = frame->channel;
  out[5] = (uint8_t)(frame->msg_id & 0xffu);
  out[6] = (uint8_t)(frame->msg_id >> 8);
  out[7] = (uint8_t)(frame->length & 0xffu);
  out[8] = (uint8_t)(frame->length >> 8);
  if (frame->length > 0) {
    memcpy(out + LINK_HEADER_LEN, frame->payload, frame->length);
  }
  crc = link_crc16(out, LINK_HEADER_LEN + frame->length);
  out[LINK_HEADER_LEN + frame->length] = (uint8_t)(crc & 0xffu);
  out[LINK_HEADER_LEN + frame->length + 1u] = (uint8_t)(crc >> 8);
  return (int)total;
}

int link_frame_decode(const uint8_t *in, size_t n, link_frame_t *frame) {
  uint16_t length;
  uint16_t crc;
  uint16_t got;
  if (!in || !frame || n < LINK_HEADER_LEN + 2u) {
    return -1;
  }
  if (in[0] != (uint8_t)(LINK_MAGIC & 0xffu) ||
      in[1] != (uint8_t)(LINK_MAGIC >> 8)) {
    return -1;
  }
  length = (uint16_t)in[7] | ((uint16_t)in[8] << 8);
  if (length > LINK_MAX_PAYLOAD || n < LINK_HEADER_LEN + length + 2u) {
    return -1;
  }
  crc = link_crc16(in, LINK_HEADER_LEN + length);
  got = (uint16_t)in[LINK_HEADER_LEN + length] |
        ((uint16_t)in[LINK_HEADER_LEN + length + 1u] << 8);
  if (crc != got) {
    return -1;
  }
  memset(frame, 0, sizeof(*frame));
  frame->version = in[2];
  frame->flags = in[3];
  frame->channel = in[4];
  frame->msg_id = (uint16_t)in[5] | ((uint16_t)in[6] << 8);
  frame->length = length;
  if (length > 0) {
    memcpy(frame->payload, in + LINK_HEADER_LEN, length);
  }
  return (int)(LINK_HEADER_LEN + length + 2u);
}

int link_tlv_put(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                 const void *value, uint8_t len) {
  size_t at;
  if (!buf || !used || (len > 0 && !value)) {
    return -1;
  }
  at = *used;
  if (at + 2u + len > cap) {
    return -1;
  }
  buf[at] = tag;
  buf[at + 1u] = len;
  if (len > 0) {
    memcpy(buf + at + 2u, value, len);
  }
  *used = at + 2u + len;
  return 0;
}

int link_tlv_put_u8(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                    uint8_t value) {
  return link_tlv_put(buf, cap, used, tag, &value, 1);
}

int link_tlv_put_u16(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                     uint16_t value) {
  uint8_t raw[2];
  raw[0] = (uint8_t)(value & 0xffu);
  raw[1] = (uint8_t)(value >> 8);
  return link_tlv_put(buf, cap, used, tag, raw, 2);
}

int link_tlv_put_u32(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                     uint32_t value) {
  uint8_t raw[4];
  raw[0] = (uint8_t)(value & 0xffu);
  raw[1] = (uint8_t)((value >> 8) & 0xffu);
  raw[2] = (uint8_t)((value >> 16) & 0xffu);
  raw[3] = (uint8_t)(value >> 24);
  return link_tlv_put(buf, cap, used, tag, raw, 4);
}

int link_tlv_put_str(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                     const char *text) {
  size_t n;
  if (!text) {
    text = "";
  }
  n = strlen(text);
  if (n > 255u) {
    return -1;
  }
  return link_tlv_put(buf, cap, used, tag, text, (uint8_t)n);
}

int link_tlv_get(const uint8_t *buf, size_t n, uint8_t tag, uint8_t *out,
                 size_t cap, uint8_t *out_len) {
  size_t i = 0;
  if (!buf || !out_len) {
    return -1;
  }
  while (i + 2u <= n) {
    uint8_t t = buf[i];
    uint8_t len = buf[i + 1u];
    if (i + 2u + len > n) {
      return -1;
    }
    if (t == tag) {
      if (len > cap || (len > 0 && !out)) {
        return -1;
      }
      if (len > 0) {
        memcpy(out, buf + i + 2u, len);
      }
      *out_len = len;
      return 0;
    }
    i += 2u + len;
  }
  return -1;
}

static int magic_at(const uint8_t *buf, size_t len, size_t i) {
  return i + 1u < len && buf[i] == (uint8_t)(LINK_MAGIC & 0xffu) &&
         buf[i + 1u] == (uint8_t)(LINK_MAGIC >> 8);
}

int link_stream_push(link_stream_t *stream, const uint8_t *data, size_t n,
                     int (*on_frame)(const uint8_t *frame, size_t len,
                                     void *user),
                     void *user) {
  size_t i;
  if (!stream || !on_frame || (n > 0 && !data)) {
    return -1;
  }
  for (i = 0; i < n; i++) {
    if (stream->len >= LINK_MAX_FRAME) {
      memmove(stream->buf, stream->buf + 1, stream->len - 1u);
      stream->len--;
    }
    stream->buf[stream->len++] = data[i];
  }
  for (;;) {
    size_t start = 0;
    uint16_t length;
    size_t total;
    link_frame_t frame;
    int used;
    while (start + 1u < stream->len &&
           !magic_at(stream->buf, stream->len, start)) {
      start++;
    }
    if (start > 0) {
      memmove(stream->buf, stream->buf + start, stream->len - start);
      stream->len -= start;
    }
    if (stream->len < LINK_HEADER_LEN) {
      return 0;
    }
    length = (uint16_t)stream->buf[7] | ((uint16_t)stream->buf[8] << 8);
    if (length > LINK_MAX_PAYLOAD) {
      memmove(stream->buf, stream->buf + 1, stream->len - 1u);
      stream->len--;
      continue;
    }
    total = LINK_HEADER_LEN + length + 2u;
    if (stream->len < total) {
      return 0;
    }
    used = link_frame_decode(stream->buf, total, &frame);
    if (used < 0) {
      memmove(stream->buf, stream->buf + 1, stream->len - 1u);
      stream->len--;
      continue;
    }
    if (on_frame(stream->buf, (size_t)used, user) != 0) {
      return -1;
    }
    memmove(stream->buf, stream->buf + (size_t)used,
            stream->len - (size_t)used);
    stream->len -= (size_t)used;
  }
}
