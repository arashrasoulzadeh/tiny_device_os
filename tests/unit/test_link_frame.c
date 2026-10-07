#include "link_frame.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_crc_of_123456789_is_ccitt_false(void) {
  const uint8_t text[] = "123456789";
  TEST_ASSERT_EQUAL_HEX16(0x29b1, link_crc16(text, 9));
}

void test_roundtrip_and_bad_crc(void) {
  link_frame_t frame;
  link_frame_t back;
  uint8_t raw[64];
  int n;
  memset(&frame, 0, sizeof(frame));
  frame.version = 1;
  frame.channel = LINK_CH_CALL;
  frame.msg_id = 0x0201;
  frame.payload[0] = 0x11;
  frame.length = 1;
  n = link_frame_encode(&frame, raw, sizeof(raw));
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL_UINT8(0x07, raw[0]);
  TEST_ASSERT_EQUAL_UINT8(0xab, raw[1]);
  TEST_ASSERT_EQUAL_INT(n, link_frame_decode(raw, (size_t)n, &back));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_CALL, back.channel);
  TEST_ASSERT_EQUAL_UINT16(0x0201, back.msg_id);
  TEST_ASSERT_EQUAL_UINT8(0x11, back.payload[0]);
  raw[n - 1] ^= 1u;
  TEST_ASSERT_EQUAL_INT(-1, link_frame_decode(raw, (size_t)n, &back));
}

static int g_frames;

static int count_frame(const uint8_t *frame, size_t len, void *user) {
  link_frame_t decoded;
  (void)user;
  TEST_ASSERT_GREATER_THAN(0, link_frame_decode(frame, len, &decoded));
  g_frames++;
  return 0;
}

void test_stream_skips_noise_and_rejects_a_huge_length(void) {
  link_frame_t frame;
  uint8_t raw[64];
  uint8_t chunk[80];
  link_stream_t stream;
  int n;
  memset(&frame, 0, sizeof(frame));
  frame.version = 1;
  frame.channel = LINK_CH_EVENT;
  frame.length = 0;
  n = link_frame_encode(&frame, raw, sizeof(raw));
  memset(&stream, 0, sizeof(stream));
  chunk[0] = 0x00;
  chunk[1] = 0x07;
  chunk[2] = 0xab;
  chunk[3] = 1;
  chunk[4] = 0;
  chunk[5] = 0;
  chunk[6] = 0;
  chunk[7] = 0;
  chunk[8] = 0xff;
  chunk[9] = 0xff;
  memcpy(chunk + 10, raw, (size_t)n);
  g_frames = 0;
  TEST_ASSERT_EQUAL_INT(
      0, link_stream_push(&stream, chunk, (size_t)n + 10u, count_frame, NULL));
  TEST_ASSERT_EQUAL_INT(1, g_frames);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_crc_of_123456789_is_ccitt_false);
  RUN_TEST(test_roundtrip_and_bad_crc);
  RUN_TEST(test_stream_skips_noise_and_rejects_a_huge_length);
  return UNITY_END();
}
