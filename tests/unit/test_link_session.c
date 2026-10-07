#include "link_session.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const uint8_t k_key[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                  0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,
                                  0xcc, 0xdd, 0xee, 0xff};

void test_hello_then_sealed_call_rejects_replay(void) {
  link_session_t host;
  link_session_t device;
  uint8_t hello[64];
  uint8_t wire[128];
  uint8_t replay[128];
  link_frame_t frame;
  link_frame_t plain;
  const uint8_t pt[] = {1, 2, 3, 4};
  int n;
  int sealed;

  link_session_init(&host, k_key, 1, LINK_DIR_HOST);
  link_session_init(&device, k_key, 1, LINK_DIR_DEVICE);
  n = link_session_encode_hello(&host, 0, 1, hello, sizeof(hello));
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_GREATER_THAN(0, link_frame_decode(hello, (size_t)n, &frame));
  TEST_ASSERT_EQUAL_INT(0, link_session_accept_hello(&device, &frame, 7));
  TEST_ASSERT_TRUE(device.open);
  TEST_ASSERT_EQUAL_UINT16(7, device.session);

  n = link_session_encode_hello(&device, device.session, 2, hello,
                                sizeof(hello));
  TEST_ASSERT_GREATER_THAN(0, link_frame_decode(hello, (size_t)n, &frame));
  TEST_ASSERT_EQUAL_INT(0, link_session_accept_hello(&host, &frame, 0));
  TEST_ASSERT_EQUAL_UINT16(7, host.session);

  sealed = link_session_seal(&host, LINK_CH_CALL, 3, pt, sizeof(pt), wire,
                             sizeof(wire));
  TEST_ASSERT_GREATER_THAN(0, sealed);
  memcpy(replay, wire, (size_t)sealed);
  TEST_ASSERT_GREATER_THAN(0, link_frame_decode(wire, (size_t)sealed, &frame));
  TEST_ASSERT_EQUAL_INT(0, link_session_open(&device, &frame, &plain));
  TEST_ASSERT_EQUAL_UINT16(sizeof(pt), plain.length);
  TEST_ASSERT_EQUAL_MEMORY(pt, plain.payload, sizeof(pt));
  TEST_ASSERT_EQUAL_INT(-1, link_session_open(&device, &frame, &plain));

  wire[LINK_HEADER_LEN] ^= 0xffu;
  TEST_ASSERT_GREATER_THAN(0,
                           link_frame_decode(replay, (size_t)sealed, &frame));
  frame.payload[0] = 9;
  TEST_ASSERT_EQUAL_INT(-1, link_session_open(&device, &frame, &plain));
}

void test_key_id_mismatch_does_not_open(void) {
  link_session_t host;
  link_session_t device;
  uint8_t hello[64];
  link_frame_t frame;
  int n;
  link_session_init(&host, k_key, 1, LINK_DIR_HOST);
  link_session_init(&device, k_key, 2, LINK_DIR_DEVICE);
  n = link_session_encode_hello(&host, 0, 1, hello, sizeof(hello));
  TEST_ASSERT_GREATER_THAN(0, link_frame_decode(hello, (size_t)n, &frame));
  TEST_ASSERT_EQUAL_INT(-1, link_session_accept_hello(&device, &frame, 1));
  TEST_ASSERT_FALSE(device.open);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hello_then_sealed_call_rejects_replay);
  RUN_TEST(test_key_id_mismatch_does_not_open);
  return UNITY_END();
}
