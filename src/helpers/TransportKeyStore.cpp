#include "TransportKeyStore.h"
#include <SHA256.h>

uint16_t TransportKey::calcTransportCode(const mesh::Packet* packet) const {
  uint16_t code;
  SHA256 sha;
  sha.resetHMAC(key, sizeof(key));
  uint8_t type = packet->getPayloadType();
  sha.update(&type, 1);
  sha.update(packet->payload, packet->payload_len);
  sha.finalizeHMAC(key, sizeof(key), &code, 2);
  return code;
}

int TransportKeyStore::loadKeysFor(const char* name, uint16_t id, TransportKey keys[], int max_num) {
  int n = 0;
  for (int i = 0; i < num_cache && n < max_num; i++) {  // first, check cache
    if (cache_ids[i] == id) {
      keys[n++] = cache_keys[i];
    }
  }
  if (n > 0) return n;   // cache hit!

  if (*name == '#') {   // is a publicly-known hashtag region
    SHA256 sha;
    sha.update(name, strlen(name));
    sha.finalize(&keys[0], sizeof(keys[0].key));
    n = 1;
  } else {
    // TODO:  retrieve from difficult-to-copy keystore
  }

  // store in cache (if room)
  for (int i = 0; i < n; i++) {
    if (num_cache < MAX_TKS_ENTRIES) {
      cache_ids[num_cache] = id;
      cache_keys[num_cache] = keys[i];
      num_cache++;
    } else {
      // TODO: evict oldest cache entry
    }
  }
  return n;
}
