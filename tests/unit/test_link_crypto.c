#include "link_crypto.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void expect_tag(const uint8_t *pt, size_t pt_len, const char *tag_hex) {
  uint8_t key[16];
  uint8_t nonce[12];
  uint8_t ct[32];
  uint8_t tag[16];
  uint8_t back[32];
  size_t i;
  memset(key, 0, sizeof(key));
  memset(nonce, 0, sizeof(nonce));
  TEST_ASSERT_EQUAL_INT(
      0, link_aesgcm_encrypt(key, nonce, NULL, 0, pt, pt_len, ct, tag));
  for (i = 0; i < 16; i++) {
    unsigned value;
    TEST_ASSERT_EQUAL_INT(1, sscanf(tag_hex + i * 2, "%02x", &value));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)value, tag[i]);
  }
  TEST_ASSERT_EQUAL_INT(
      0, link_aesgcm_decrypt(key, nonce, NULL, 0, ct, pt_len, tag, back));
  if (pt_len > 0) {
    TEST_ASSERT_EQUAL_MEMORY(pt, back, pt_len);
  }
  tag[0] ^= 1u;
  TEST_ASSERT_EQUAL_INT(
      -1, link_aesgcm_decrypt(key, nonce, NULL, 0, ct, pt_len, tag, back));
}

void test_empty_plaintext_matches_nist(void) {
  expect_tag(NULL, 0, "58e2fccefa7e3061367f1d57a4e7455a");
}

void test_sixteen_zero_bytes_match_nist(void) {
  uint8_t pt[16];
  uint8_t key[16];
  uint8_t nonce[12];
  uint8_t ct[16];
  uint8_t tag[16];
  const char *ct_hex = "0388dace60b6a392f328c2b971b2fe78";
  const char *tag_hex = "ab6e47d42cec13bdf53a67b21257bddf";
  size_t i;
  memset(pt, 0, sizeof(pt));
  memset(key, 0, sizeof(key));
  memset(nonce, 0, sizeof(nonce));
  TEST_ASSERT_EQUAL_INT(
      0, link_aesgcm_encrypt(key, nonce, NULL, 0, pt, sizeof(pt), ct, tag));
  for (i = 0; i < 16; i++) {
    unsigned ct_b;
    unsigned tag_b;
    TEST_ASSERT_EQUAL_INT(1, sscanf(ct_hex + i * 2, "%02x", &ct_b));
    TEST_ASSERT_EQUAL_INT(1, sscanf(tag_hex + i * 2, "%02x", &tag_b));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)ct_b, ct[i]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)tag_b, tag[i]);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_plaintext_matches_nist);
  RUN_TEST(test_sixteen_zero_bytes_match_nist);
  return UNITY_END();
}
