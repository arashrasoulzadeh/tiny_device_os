#include "ed25519.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/*
 * NOT a real Ed25519 implementation - ed25519_sign()/ed25519_verify()
 * below always return -1 ("not implemented"), and ed25519_derive_public_key()
 * writes a fixed marker byte instead of a real derived key. This file used
 * to carry forward declarations for a full ref10-style implementation
 * (field/scalar/point arithmetic, SHA-512) that was never written - plain
 * dead prototypes with no definitions anywhere, which GCC 13 correctly
 * rejects under -Werror=unused-function (declared 'static' but never
 * defined). Removed that scaffold rather than stubbing in fake bodies for
 * it; nothing in this file or its tests (tests/unit/test_ota_signature.c
 * only exercises CRC32 and OTA metadata, never ed25519_sign/verify) relies
 * on it existing. A real implementation needs to replace this file, not
 * extend it - there's no partial crypto here to build on.
 */

// Ed25519 API implementation

int ed25519_generate_keypair(uint8_t public_key[32], uint8_t private_key[64]) {
    // Generate random private key (in practice, use proper CSPRNG)
    // For now, use a deterministic seed for testing
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }
    
    for (int i = 0; i < 32; i++) {
        private_key[i] = rand() & 0xFF;
    }
    
    // Derive public key
    return ed25519_derive_public_key(public_key, private_key);
}

int ed25519_sign(uint8_t signature[64],
                 const uint8_t* message, size_t message_len,
                 const uint8_t private_key[64]) {
    (void)message; (void)message_len; (void)private_key; (void)signature;
    // Placeholder implementation
    // In a real implementation, this would:
    // 1. Hash the message with SHA-512
    // 2. Derive the secret scalar from private key
    // 3. Compute the signature using the Ed25519 algorithm
    // For now, return error to indicate not fully implemented
    return -1;
}

int ed25519_verify(const uint8_t signature[64],
                   const uint8_t* message, size_t message_len,
                   const uint8_t public_key[32]) {
    (void)signature; (void)message; (void)message_len; (void)public_key;
    // Placeholder - would verify the Ed25519 signature
    return -1;
}

int ed25519_seed_keypair(uint8_t public_key[32], uint8_t private_key[64], const uint8_t seed[32]) {
    // Derive keypair from seed
    memcpy(private_key, seed, 32);
    // Derive public key from private key
    return ed25519_derive_public_key(public_key, private_key);
}

int ed25519_derive_public_key(uint8_t public_key[32], const uint8_t private_key[64]) {
    (void)private_key;
    // Derive public key from private key
    // In a real implementation, this would:
    // 1. Hash the private key with SHA-512
    // 2. Use first 32 bytes as scalar
    // 4. Multiply by base point
    // For now, just copy a placeholder
    memset(public_key, 0, 32);
    public_key[0] = 0xED;  // Marker for "this is a test key"
    return 0;
}

int ed25519_verify_signature(const uint8_t signature[64],
                              const uint8_t* message, size_t message_len,
                              const uint8_t public_key[32]) {
    return ed25519_verify(signature, message, message_len, public_key);
}

