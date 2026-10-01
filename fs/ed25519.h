#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ED25519_PUBLIC_KEY_SIZE 32
#define ED25519_PRIVATE_KEY_SIZE 64
#define ED25519_SIGNATURE_SIZE 64

// Generate a new Ed25519 key pair
// Returns 0 on success, -1 on failure
int ed25519_generate_keypair(uint8_t public_key[ED25519_PUBLIC_KEY_SIZE],
                              uint8_t private_key[ED25519_PRIVATE_KEY_SIZE]);

// Sign a message with Ed25519 private key
// Returns 0 on success, -1 on failure
int ed25519_sign(uint8_t signature[ED25519_SIGNATURE_SIZE],
                 const uint8_t* message, size_t message_len,
                 const uint8_t private_key[ED25519_PRIVATE_KEY_SIZE]);

// Verify an Ed25519 signature
// Returns 0 on success (valid signature), -1 on failure
int ed25519_verify(const uint8_t signature[ED25519_SIGNATURE_SIZE],
                   const uint8_t* message, size_t message_len,
                   const uint8_t public_key[ED25519_PUBLIC_KEY_SIZE]);

// Generate a key pair from a seed (for deterministic key generation)
int ed25519_seed_keypair(uint8_t public_key[ED25519_PUBLIC_KEY_SIZE],
                          uint8_t private_key[ED25519_PRIVATE_KEY_SIZE],
                          const uint8_t seed[32]);

// Derive public key from private key
int ed25519_derive_public_key(uint8_t public_key[ED25519_PUBLIC_KEY_SIZE],
                               const uint8_t private_key[ED25519_PRIVATE_KEY_SIZE]);

// Verify a signature using public key (alias for ed25519_verify)
int ed25519_verify_signature(const uint8_t signature[ED25519_SIGNATURE_SIZE],
                              const uint8_t* message, size_t message_len,
                              const uint8_t public_key[ED25519_PUBLIC_KEY_SIZE]);

#ifdef __cplusplus
}
#endif