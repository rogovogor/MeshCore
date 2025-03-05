#include "Identity.h"
#include <string.h>
#define ED25519_NO_SEED  1
#include <ed_25519.h>

// For ESP32, we use libsodium for cryptographic operations to reduce stack usage
#ifdef USE_ESP32_ENCRYPTION
#include <sodium.h>
#endif

namespace mesh {

Identity::Identity() {
  memset(pub_key, 0, sizeof(pub_key));
}

Identity::Identity(const char* pub_hex) {
  Utils::fromHex(pub_key, PUB_KEY_SIZE, pub_hex);
}

bool Identity::verify(const uint8_t* sig, const uint8_t* message, int msg_len) const {
  #ifdef USE_ESP32_ENCRYPTION
    // Using libsodium for verification on ESP32 to reduce stack usage
    // This function performs signature verification with much lower stack requirements
    // than the default implementation
    return crypto_sign_ed25519_verify_detached(sig, message, msg_len, pub_key) == 0;
  #else
    return ed25519_verify(sig, message, msg_len, pub_key);
  #endif
}

bool Identity::readFrom(Stream& s) {
  return (s.readBytes(pub_key, PUB_KEY_SIZE) == PUB_KEY_SIZE);
}

bool Identity::writeTo(Stream& s) const {
  return (s.write(pub_key, PUB_KEY_SIZE) == PUB_KEY_SIZE);
}

void Identity::printTo(Stream& s) const {
  Utils::printHex(s, pub_key, PUB_KEY_SIZE);
}

LocalIdentity::LocalIdentity() {
  memset(prv_key, 0, sizeof(prv_key));
}

LocalIdentity::LocalIdentity(const char* prv_hex, const char* pub_hex) : Identity(pub_hex) {
  Utils::fromHex(prv_key, PRV_KEY_SIZE, prv_hex);
}

LocalIdentity::LocalIdentity(RNG* rng) {
  uint8_t seed[SEED_SIZE];
  rng->random(seed, SEED_SIZE);
  
  #ifdef USE_ESP32_ENCRYPTION
    // Use libsodium for keypair generation on ESP32 to reduce stack usage
    // NOTE: Format differences between implementations:
    // - The current ed25519 implementation (orlp/ed25519) uses a 64-byte private key format
    // - Libsodium also uses a 64-byte format for Ed25519 secret keys, where:
    //   * First 32 bytes: the actual private key seed
    //   * Last 32 bytes: the corresponding public key
    
    // Generate keypair using libsodium with the provided seed
    // This avoids the deep stack usage of the default implementation
    crypto_sign_ed25519_seed_keypair(pub_key, prv_key, seed);
  #else
    ed25519_create_keypair(pub_key, prv_key, seed);
  #endif
}

bool LocalIdentity::readFrom(Stream& s) {
  bool success = (s.readBytes(pub_key, PUB_KEY_SIZE) == PUB_KEY_SIZE);
  success = success && (s.readBytes(prv_key, PRV_KEY_SIZE) == PRV_KEY_SIZE);
  return success;
}

bool LocalIdentity::writeTo(Stream& s) const {
  bool success = (s.write(pub_key, PUB_KEY_SIZE) == PUB_KEY_SIZE);
  success = success && (s.write(prv_key, PRV_KEY_SIZE) == PRV_KEY_SIZE);
  return success;
}

void LocalIdentity::printTo(Stream& s) const {
  s.print("pub_key: "); Utils::printHex(s, pub_key, PUB_KEY_SIZE); s.println();
  s.print("prv_key: "); Utils::printHex(s, prv_key, PRV_KEY_SIZE); s.println();
}

size_t LocalIdentity::writeTo(uint8_t* dest, size_t max_len) {
  if (max_len < PRV_KEY_SIZE) return 0;  // not big enough

  if (max_len < PRV_KEY_SIZE + PUB_KEY_SIZE) {  // only room for prv_key
    memcpy(dest, prv_key, PRV_KEY_SIZE);
    return PRV_KEY_SIZE;
  }
  memcpy(dest, prv_key, PRV_KEY_SIZE);  // otherwise can fit prv + pub keys
  memcpy(&dest[PRV_KEY_SIZE], pub_key, PUB_KEY_SIZE);
  return PRV_KEY_SIZE + PUB_KEY_SIZE;
}

void LocalIdentity::readFrom(const uint8_t* src, size_t len) {
  if (len == PRV_KEY_SIZE + PUB_KEY_SIZE) {  // has prv + pub keys
    memcpy(prv_key, src, PRV_KEY_SIZE);
    memcpy(pub_key, &src[PRV_KEY_SIZE], PUB_KEY_SIZE);
  } else if (len == PRV_KEY_SIZE) {
    memcpy(prv_key, src, PRV_KEY_SIZE);
    
  #ifdef USE_ESP32_ENCRYPTION
      // In libsodium, the private key already contains the public key in its last 32 bytes
      // We can just extract it directly, avoiding the expensive derivation calculation
      // This significantly reduces stack usage on ESP32
      memcpy(pub_key, prv_key + 32, 32);
  #else
      // now need to re-calculate the pub_key
      ed25519_derive_pub(pub_key, prv_key);
  #endif
  }
}

void LocalIdentity::sign(uint8_t* sig, const uint8_t* message, int msg_len) const {
  #ifdef USE_ESP32_ENCRYPTION
    // Use libsodium for signing on ESP32 to reduce stack usage
    // The libsodium implementation uses less stack space than the default ed25519 implementation
    crypto_sign_ed25519_detached(sig, NULL, message, msg_len, prv_key);
  #else
    ed25519_sign(sig, message, msg_len, pub_key, prv_key);
  #endif
}

void LocalIdentity::calcSharedSecret(uint8_t* secret, const uint8_t* other_pub_key) {
  #ifdef USE_ESP32_ENCRYPTION
    // NOTE: To calculate a shared secret with Ed25519 keys and libsodium, we need to:
    // 1. Convert the Ed25519 keys to Curve25519 (X25519) format
    // 2. Perform the key exchange using the converted keys
    //
    // The default implementation handles this conversion internally,
    // but with libsodium we need to do these steps explicitly.
    // This approach uses less stack space compared to the original implementation.
    
    unsigned char x25519_pk[crypto_scalarmult_curve25519_BYTES];
    unsigned char x25519_sk[crypto_scalarmult_curve25519_BYTES];
    
    // Convert Ed25519 keys to Curve25519 keys for ECDH
    crypto_sign_ed25519_pk_to_curve25519(x25519_pk, other_pub_key);
    crypto_sign_ed25519_sk_to_curve25519(x25519_sk, prv_key);
    
    // Calculate shared secret using X25519
    crypto_scalarmult_curve25519(secret, x25519_sk, x25519_pk);
  #else
    ed25519_key_exchange(secret, other_pub_key, prv_key);
  #endif
}

}