#include "app.h"
#include "link_bridge.h"
#include "link_session.h"
#include "notify_service.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const uint8_t k_key[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                  0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,
                                  0xcc, 0xdd, 0xee, 0xff};

static void handshake(link_session_t *host) {
  uint8_t hello[80];
  uint8_t tx[512];
  link_frame_t frame;
  int n;
  int rn;
  link_session_init(host, k_key, 1, LINK_DIR_HOST);
  n = link_session_encode_hello(host, 0, 1, hello, sizeof(hello));
  TEST_ASSERT_GREATER_THAN(0, n);
  rn = link_bridge_rx(hello, (size_t)n, tx, sizeof(tx));
  TEST_ASSERT_GREATER_THAN(0, rn);
  TEST_ASSERT_GREATER_THAN(0, link_frame_decode(tx, (size_t)rn, &frame));
  TEST_ASSERT_EQUAL_INT(0, link_session_accept_hello(host, &frame, 0));
}

static int seal_call(link_session_t *host, const uint8_t *pt, size_t pt_len,
                     uint8_t *tx, size_t cap, link_frame_t *plain) {
  uint8_t wire[LINK_MAX_FRAME];
  link_frame_t frame;
  int n =
      link_session_seal(host, LINK_CH_CALL, 3, pt, pt_len, wire, sizeof(wire));
  int rn;
  TEST_ASSERT_GREATER_THAN(0, n);
  rn = link_bridge_rx(wire, (size_t)n, tx, cap);
  TEST_ASSERT_GREATER_THAN(0, rn);
  TEST_ASSERT_GREATER_THAN(0, link_frame_decode(tx, (size_t)rn, &frame));
  return link_session_open(host, &frame, plain);
}

void test_notify_message_key_and_sensor(void) {
  link_session_t host;
  uint8_t payload[128];
  uint8_t tx[LINK_MAX_FRAME];
  link_frame_t reply;
  link_frame_t plain;
  size_t used;
  char text[64];
  app_manifest_t manifest;

  notify_service_start();
  notify_service_set_now_ms(0);
  TEST_ASSERT_EQUAL_INT(0, app_init());
  memset(&manifest, 0, sizeof(manifest));
  memcpy(manifest.name, "sensors", 8);
  TEST_ASSERT_EQUAL_INT(0, app_install_manifest(&manifest, "sensors"));

  link_bridge_init(k_key, 1);
  handshake(&host);

  used = 0;
  TEST_ASSERT_EQUAL_INT(0,
                        link_tlv_put_u8(payload, sizeof(payload), &used,
                                        LINK_TAG_METHOD, LINK_METHOD_NOTIFY));
  TEST_ASSERT_EQUAL_INT(0, link_tlv_put_str(payload, sizeof(payload), &used,
                                            LINK_TAG_TEXT, "hello world"));
  TEST_ASSERT_EQUAL_INT(
      0, seal_call(&host, payload, used, tx, sizeof(tx), &plain));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_RESULT, plain.channel);
  TEST_ASSERT_EQUAL_STRING("hello world", notify_service_title());

  used = 0;
  TEST_ASSERT_EQUAL_INT(0,
                        link_tlv_put_u8(payload, sizeof(payload), &used,
                                        LINK_TAG_METHOD, LINK_METHOD_MESSAGE));
  TEST_ASSERT_EQUAL_INT(0, link_tlv_put_str(payload, sizeof(payload), &used,
                                            LINK_TAG_APP, "missing"));
  TEST_ASSERT_EQUAL_INT(0, link_tlv_put_str(payload, sizeof(payload), &used,
                                            LINK_TAG_TEXT, "nope"));
  TEST_ASSERT_EQUAL_INT(
      0, seal_call(&host, payload, used, tx, sizeof(tx), &plain));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_ERROR, plain.channel);
  TEST_ASSERT_EQUAL_INT(-1, link_message_take("missing", text, sizeof(text)));

  used = 0;
  TEST_ASSERT_EQUAL_INT(0,
                        link_tlv_put_u8(payload, sizeof(payload), &used,
                                        LINK_TAG_METHOD, LINK_METHOD_MESSAGE));
  TEST_ASSERT_EQUAL_INT(0, link_tlv_put_str(payload, sizeof(payload), &used,
                                            LINK_TAG_APP, "sensors"));
  TEST_ASSERT_EQUAL_INT(0, link_tlv_put_str(payload, sizeof(payload), &used,
                                            LINK_TAG_TEXT, "battery low"));
  TEST_ASSERT_EQUAL_INT(
      0, seal_call(&host, payload, used, tx, sizeof(tx), &plain));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_RESULT, plain.channel);
  TEST_ASSERT_EQUAL_INT(0, link_message_take("sensors", text, sizeof(text)));
  TEST_ASSERT_EQUAL_STRING("battery low", text);

  used = 0;
  TEST_ASSERT_EQUAL_INT(0, link_tlv_put_u8(payload, sizeof(payload), &used,
                                           LINK_TAG_METHOD, LINK_METHOD_KEY));
  TEST_ASSERT_EQUAL_INT(0, link_tlv_put_str(payload, sizeof(payload), &used,
                                            LINK_TAG_TEXT, "enter"));
  TEST_ASSERT_EQUAL_INT(
      0, link_tlv_put_u8(payload, sizeof(payload), &used, LINK_TAG_PRESSED, 1));
  TEST_ASSERT_EQUAL_INT(
      0, seal_call(&host, payload, used, tx, sizeof(tx), &plain));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_RESULT, plain.channel);

  TEST_ASSERT_EQUAL_INT(0, link_bridge_poll(1000, tx, sizeof(tx)));
  (void)reply;
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_notify_message_key_and_sensor);
  return UNITY_END();
}
