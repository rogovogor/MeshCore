#pragma once

#include <Arduino.h>   // needed for PlatformIO
#include <Packet.h>
#include <helpers/IdentityStore.h>

struct TransportKey {
  uint8_t key[16];

  uint16_t calcTransportCode(const mesh::Packet* packet) const;
};

#define MAX_TKS_ENTRIES   16

class TransportKeyStore {
  uint16_t     cache_ids[MAX_TKS_ENTRIES];
  TransportKey cache_keys[MAX_TKS_ENTRIES];
  int num_cache;

public:
  TransportKeyStore() { num_cache = 0; }
  int loadKeysFor(const char* name, uint16_t id, TransportKey keys[], int max_num);
};
