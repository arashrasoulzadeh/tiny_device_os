#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** AES-128-GCM. `nonce` is 12 bytes. Tag is 16 bytes. Returns 0, or -1 if the
 * tag does not match. */
int link_aesgcm_encrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aad_len, const uint8_t *pt,
                        size_t pt_len, uint8_t *ct, uint8_t tag[16]);

int link_aesgcm_decrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aad_len, const uint8_t *ct,
                        size_t ct_len, const uint8_t tag[16], uint8_t *pt);

#ifdef __cplusplus
}
#endif
