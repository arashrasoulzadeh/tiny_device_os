#pragma once

/**
 * Link frame. Little-endian. Magic 0xAB07.
 *
 *   0  magic u16
 *   2  version u8
 *   3  flags u8
 *   4  channel u8
 *   5  msg_id u16
 *   7  length u16
 *   9  payload[length]
 *   9+length  crc16 over the bytes before it (CRC-16/CCITT-FALSE)
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_MAGIC 0xAB07u
#define LINK_VERSION 1u
#define LINK_MAX_PAYLOAD 240u
#define LINK_HEADER_LEN 9u
#define LINK_MAX_FRAME (LINK_HEADER_LEN + LINK_MAX_PAYLOAD + 2u)

#define LINK_FLAG_ACK 0x01u
#define LINK_FLAG_ERROR 0x02u
#define LINK_FLAG_MORE 0x04u
#define LINK_FLAG_ENCRYPTED 0x08u

#define LINK_CH_HELLO 0u
#define LINK_CH_CALL 1u
#define LINK_CH_RESULT 2u
#define LINK_CH_ERROR 3u
#define LINK_CH_EVENT 4u
#define LINK_CH_TTY 5u

#define LINK_METHOD_NOTIFY 1u
#define LINK_METHOD_MESSAGE 2u
#define LINK_METHOD_EVENT 3u
#define LINK_METHOD_KEY 4u

/* Tags are per payload. A call uses METHOD plus the fields that call needs. */
#define LINK_TAG_METHOD 1u
#define LINK_TAG_APP 2u
#define LINK_TAG_TEXT 3u
#define LINK_TAG_BODY 4u
#define LINK_TAG_EXTENT 5u
#define LINK_TAG_DURATION 6u
#define LINK_TAG_PRESSED 7u

#define LINK_TAG_VERSION 1u
#define LINK_TAG_KEY_ID 2u
#define LINK_TAG_SESSION 3u

#define LINK_TAG_STATUS 1u
#define LINK_TAG_CODE 1u
#define LINK_TAG_ERRMSG 2u

#define LINK_TAG_SENSOR_KEY 1u
#define LINK_TAG_SENSOR_VALUE 2u

#define LINK_EXTENT_BAND 0u
#define LINK_EXTENT_FULL 1u

#define LINK_ERR_UNKNOWN_APP 1u
#define LINK_ERR_BAD_ARGS 2u
#define LINK_ERR_UNKNOWN_METHOD 3u
#define LINK_ERR_BUSY 4u

typedef struct {
  uint8_t version;
  uint8_t flags;
  uint8_t channel;
  uint16_t msg_id;
  uint16_t length;
  uint8_t payload[LINK_MAX_PAYLOAD];
} link_frame_t;

typedef struct {
  uint8_t buf[LINK_MAX_FRAME];
  size_t len;
} link_stream_t;

uint16_t link_crc16(const uint8_t *data, size_t len);

int link_frame_encode(const link_frame_t *frame, uint8_t *out, size_t cap);
int link_frame_decode(const uint8_t *in, size_t n, link_frame_t *frame);

int link_tlv_put(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                 const void *value, uint8_t len);
int link_tlv_put_u8(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                    uint8_t value);
int link_tlv_put_u16(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                     uint16_t value);
int link_tlv_put_u32(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                     uint32_t value);
int link_tlv_put_str(uint8_t *buf, size_t cap, size_t *used, uint8_t tag,
                     const char *text);

/** Copies the first matching tag. Returns 0, or -1 if missing or too long. */
int link_tlv_get(const uint8_t *buf, size_t n, uint8_t tag, uint8_t *out,
                 size_t cap, uint8_t *out_len);

int link_stream_push(link_stream_t *stream, const uint8_t *data, size_t n,
                     int (*on_frame)(const uint8_t *frame, size_t len,
                                     void *user),
                     void *user);

#ifdef __cplusplus
}
#endif
