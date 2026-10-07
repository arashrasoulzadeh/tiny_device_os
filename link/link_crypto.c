#include "link_crypto.h"

#include <string.h>

static const uint8_t k_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
    0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
    0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
    0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
    0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
    0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
    0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
    0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
    0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
    0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
    0xb0, 0x54, 0xbb, 0x16,
};

static const uint8_t k_rcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10,
                                   0x20, 0x40, 0x80, 0x1b, 0x36};

static uint8_t xtime(uint8_t x) {
  return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0));
}

static void expand_key(const uint8_t key[16], uint8_t round_key[176]) {
  int i;
  memcpy(round_key, key, 16);
  for (i = 4; i < 44; i++) {
    uint8_t temp[4];
    int j;
    memcpy(temp, round_key + (i - 1) * 4, 4);
    if ((i % 4) == 0) {
      uint8_t t = temp[0];
      temp[0] = k_sbox[temp[1]];
      temp[1] = k_sbox[temp[2]];
      temp[2] = k_sbox[temp[3]];
      temp[3] = k_sbox[t];
      temp[0] ^= k_rcon[i / 4];
    }
    for (j = 0; j < 4; j++) {
      round_key[i * 4 + j] = round_key[(i - 4) * 4 + j] ^ temp[j];
    }
  }
}

static void add_round_key(uint8_t state[16], const uint8_t *round_key) {
  int i;
  for (i = 0; i < 16; i++) {
    state[i] ^= round_key[i];
  }
}

static void sub_bytes(uint8_t state[16]) {
  int i;
  for (i = 0; i < 16; i++) {
    state[i] = k_sbox[state[i]];
  }
}

static void shift_rows(uint8_t state[16]) {
  uint8_t t;
  t = state[1];
  state[1] = state[5];
  state[5] = state[9];
  state[9] = state[13];
  state[13] = t;
  t = state[2];
  state[2] = state[10];
  state[10] = t;
  t = state[6];
  state[6] = state[14];
  state[14] = t;
  t = state[15];
  state[15] = state[11];
  state[11] = state[7];
  state[7] = state[3];
  state[3] = t;
}

static void mix_columns(uint8_t state[16]) {
  int c;
  for (c = 0; c < 4; c++) {
    uint8_t *col = state + c * 4;
    uint8_t a0 = col[0];
    uint8_t a1 = col[1];
    uint8_t a2 = col[2];
    uint8_t a3 = col[3];
    uint8_t r0 = (uint8_t)(xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3);
    uint8_t r1 = (uint8_t)(a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3);
    uint8_t r2 = (uint8_t)(a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3));
    uint8_t r3 = (uint8_t)((xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3));
    col[0] = r0;
    col[1] = r1;
    col[2] = r2;
    col[3] = r3;
  }
}

static void aes_encrypt_block(const uint8_t round_key[176],
                              const uint8_t in[16], uint8_t out[16]) {
  uint8_t state[16];
  int round;
  memcpy(state, in, 16);
  add_round_key(state, round_key);
  for (round = 1; round <= 9; round++) {
    sub_bytes(state);
    shift_rows(state);
    mix_columns(state);
    add_round_key(state, round_key + round * 16);
  }
  sub_bytes(state);
  shift_rows(state);
  add_round_key(state, round_key + 160);
  memcpy(out, state, 16);
}

static void xor_block(uint8_t *dst, const uint8_t *src) {
  int i;
  for (i = 0; i < 16; i++) {
    dst[i] ^= src[i];
  }
}

static void shift_right_block(uint8_t v[16]) {
  int i;
  uint8_t lsb = (uint8_t)(v[15] & 1u);
  for (i = 15; i > 0; i--) {
    v[i] = (uint8_t)((v[i] >> 1) | (v[i - 1] << 7));
  }
  v[0] = (uint8_t)(v[0] >> 1);
  if (lsb) {
    v[0] ^= 0xe1;
  }
}

static void gcm_mul(uint8_t x[16], const uint8_t h[16]) {
  uint8_t z[16];
  uint8_t v[16];
  int i;
  int bit;
  memset(z, 0, sizeof(z));
  memcpy(v, h, 16);
  for (i = 0; i < 16; i++) {
    for (bit = 0; bit < 8; bit++) {
      if (x[i] & (uint8_t)(1u << (7 - bit))) {
        xor_block(z, v);
      }
      shift_right_block(v);
    }
  }
  memcpy(x, z, 16);
}

static void ghash_update(uint8_t y[16], const uint8_t h[16],
                         const uint8_t *data, size_t len) {
  size_t off = 0;
  while (off < len) {
    uint8_t block[16];
    size_t n = len - off;
    int i;
    if (n > 16) {
      n = 16;
    }
    memset(block, 0, sizeof(block));
    memcpy(block, data + off, n);
    for (i = 0; i < 16; i++) {
      y[i] ^= block[i];
    }
    gcm_mul(y, h);
    off += n;
  }
}

static void put_be64(uint8_t *out, uint64_t value) {
  int i;
  for (i = 7; i >= 0; i--) {
    out[i] = (uint8_t)(value & 0xffu);
    value >>= 8;
  }
}

static void inc32(uint8_t counter[16]) {
  int i;
  for (i = 15; i >= 12; i--) {
    counter[i]++;
    if (counter[i] != 0) {
      break;
    }
  }
}

static void gcm_tag(const uint8_t round_key[176], const uint8_t h[16],
                    const uint8_t j0[16], const uint8_t *aad, size_t aad_len,
                    const uint8_t *ct, size_t ct_len, uint8_t tag[16]) {
  uint8_t y[16];
  uint8_t lengths[16];
  uint8_t ek[16];
  memset(y, 0, sizeof(y));
  if (aad_len > 0) {
    ghash_update(y, h, aad, aad_len);
  }
  if (ct_len > 0) {
    ghash_update(y, h, ct, ct_len);
  }
  memset(lengths, 0, sizeof(lengths));
  put_be64(lengths, (uint64_t)aad_len * 8u);
  put_be64(lengths + 8, (uint64_t)ct_len * 8u);
  ghash_update(y, h, lengths, 16);
  aes_encrypt_block(round_key, j0, ek);
  xor_block(y, ek);
  memcpy(tag, y, 16);
}

static void ctr_xor(const uint8_t round_key[176], uint8_t counter[16],
                    const uint8_t *in, uint8_t *out, size_t len) {
  size_t off = 0;
  while (off < len) {
    uint8_t stream[16];
    size_t n = len - off;
    size_t i;
    if (n > 16) {
      n = 16;
    }
    inc32(counter);
    aes_encrypt_block(round_key, counter, stream);
    for (i = 0; i < n; i++) {
      out[off + i] = (uint8_t)(in[off + i] ^ stream[i]);
    }
    off += n;
  }
}

int link_aesgcm_encrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aad_len, const uint8_t *pt,
                        size_t pt_len, uint8_t *ct, uint8_t tag[16]) {
  uint8_t round_key[176];
  uint8_t h[16];
  uint8_t j0[16];
  uint8_t counter[16];
  uint8_t zero[16];
  if (!key || !nonce || !tag || (pt_len > 0 && (!pt || !ct)) ||
      (aad_len > 0 && !aad)) {
    return -1;
  }
  expand_key(key, round_key);
  memset(zero, 0, sizeof(zero));
  aes_encrypt_block(round_key, zero, h);
  memset(j0, 0, sizeof(j0));
  memcpy(j0, nonce, 12);
  j0[15] = 1;
  memcpy(counter, j0, 16);
  if (pt_len > 0) {
    ctr_xor(round_key, counter, pt, ct, pt_len);
  }
  gcm_tag(round_key, h, j0, aad, aad_len, ct, pt_len, tag);
  return 0;
}

int link_aesgcm_decrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aad_len, const uint8_t *ct,
                        size_t ct_len, const uint8_t tag[16], uint8_t *pt) {
  uint8_t round_key[176];
  uint8_t h[16];
  uint8_t j0[16];
  uint8_t counter[16];
  uint8_t zero[16];
  uint8_t expect[16];
  uint8_t diff = 0;
  int i;
  if (!key || !nonce || !tag || (ct_len > 0 && (!ct || !pt)) ||
      (aad_len > 0 && !aad)) {
    return -1;
  }
  expand_key(key, round_key);
  memset(zero, 0, sizeof(zero));
  aes_encrypt_block(round_key, zero, h);
  memset(j0, 0, sizeof(j0));
  memcpy(j0, nonce, 12);
  j0[15] = 1;
  gcm_tag(round_key, h, j0, aad, aad_len, ct, ct_len, expect);
  for (i = 0; i < 16; i++) {
    diff |= (uint8_t)(expect[i] ^ tag[i]);
  }
  if (diff != 0) {
    return -1;
  }
  memcpy(counter, j0, 16);
  if (ct_len > 0) {
    ctr_xor(round_key, counter, ct, pt, ct_len);
  }
  return 0;
}
