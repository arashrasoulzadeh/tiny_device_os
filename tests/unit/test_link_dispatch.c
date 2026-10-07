#include "link_dispatch.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static char g_title[64];
static char g_app[32];
static char g_text[64];
static int g_key;
static int g_pressed;

static int on_notify(const char *title, const char *body, int extent,
                     uint32_t duration_ms, void *user) {
  (void)body;
  (void)user;
  snprintf(g_title, sizeof(g_title), "%s:%d:%u", title, extent, duration_ms);
  return 0;
}

static int on_message(const char *app, const char *text, void *user) {
  (void)user;
  snprintf(g_app, sizeof(g_app), "%s", app);
  snprintf(g_text, sizeof(g_text), "%s", text);
  return strcmp(app, "sensors") == 0 ? 0 : -1;
}

static int on_event(const char *app, const char *name, const uint8_t *payload,
                    size_t len, void *user) {
  (void)payload;
  (void)len;
  (void)user;
  snprintf(g_app, sizeof(g_app), "%s", app);
  snprintf(g_text, sizeof(g_text), "%s", name);
  return 0;
}

static int on_key(const char *key, int pressed, void *user) {
  (void)user;
  g_key = strcmp(key, "enter") == 0 ? 1 : 0;
  g_pressed = pressed;
  return g_key ? 0 : -1;
}

static link_handlers_t handlers(void) {
  link_handlers_t h;
  memset(&h, 0, sizeof(h));
  h.notify = on_notify;
  h.message = on_message;
  h.event = on_event;
  h.key = on_key;
  return h;
}

static void call_method(uint8_t method, const char *app, const char *text,
                        uint8_t *payload, size_t *len) {
  size_t used = 0;
  TEST_ASSERT_EQUAL_INT(
      0, link_tlv_put_u8(payload, 128, &used, LINK_TAG_METHOD, method));
  if (app) {
    TEST_ASSERT_EQUAL_INT(
        0, link_tlv_put_str(payload, 128, &used, LINK_TAG_APP, app));
  }
  if (text) {
    TEST_ASSERT_EQUAL_INT(
        0, link_tlv_put_str(payload, 128, &used, LINK_TAG_TEXT, text));
  }
  *len = used;
}

void test_notify_message_event_and_key(void) {
  link_handlers_t h = handlers();
  uint8_t payload[128];
  uint8_t reply[64];
  size_t len = 0;
  size_t reply_len = 0;
  uint8_t channel = 0;
  uint8_t pressed = 1;

  call_method(LINK_METHOD_NOTIFY, NULL, "hello world", payload, &len);
  TEST_ASSERT_EQUAL_INT(0, link_dispatch_call(&h, payload, len, reply,
                                              sizeof(reply), &reply_len,
                                              &channel));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_RESULT, channel);
  TEST_ASSERT_EQUAL_STRING("hello world:0:0", g_title);

  call_method(LINK_METHOD_MESSAGE, "nope", "battery low", payload, &len);
  TEST_ASSERT_EQUAL_INT(0, link_dispatch_call(&h, payload, len, reply,
                                              sizeof(reply), &reply_len,
                                              &channel));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_ERROR, channel);

  call_method(LINK_METHOD_MESSAGE, "sensors", "battery low", payload, &len);
  TEST_ASSERT_EQUAL_INT(0, link_dispatch_call(&h, payload, len, reply,
                                              sizeof(reply), &reply_len,
                                              &channel));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_RESULT, channel);
  TEST_ASSERT_EQUAL_STRING("sensors", g_app);
  TEST_ASSERT_EQUAL_STRING("battery low", g_text);

  call_method(LINK_METHOD_EVENT, "pomodoro", "skip", payload, &len);
  TEST_ASSERT_EQUAL_INT(0, link_dispatch_call(&h, payload, len, reply,
                                              sizeof(reply), &reply_len,
                                              &channel));
  TEST_ASSERT_EQUAL_STRING("skip", g_text);

  call_method(LINK_METHOD_KEY, NULL, "enter", payload, &len);
  TEST_ASSERT_EQUAL_INT(0, link_tlv_put_u8(payload, sizeof(payload), &len,
                                           LINK_TAG_PRESSED, pressed));
  TEST_ASSERT_EQUAL_INT(0, link_dispatch_call(&h, payload, len, reply,
                                              sizeof(reply), &reply_len,
                                              &channel));
  TEST_ASSERT_EQUAL_INT(1, g_key);
  TEST_ASSERT_EQUAL_INT(1, g_pressed);

  call_method(9, NULL, NULL, payload, &len);
  TEST_ASSERT_EQUAL_INT(0, link_dispatch_call(&h, payload, len, reply,
                                              sizeof(reply), &reply_len,
                                              &channel));
  TEST_ASSERT_EQUAL_UINT8(LINK_CH_ERROR, channel);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_notify_message_event_and_key);
  return UNITY_END();
}
