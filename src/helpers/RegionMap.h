#pragma once

#include <Arduino.h>   // needed for PlatformIO
#include <Packet.h>
#include "TransportKeyStore.h"

#ifndef MAX_REGION_ENTRIES
  #define MAX_REGION_ENTRIES  32
#endif

#define REGION_ALLOW_FLOOD  0x01

struct RegionEntry {
  uint16_t id;
  uint16_t parent;
  uint8_t flags;
  char name[31];
};

class RegionMap {
  TransportKeyStore* _store;
  uint16_t next_id;
  uint16_t num_regions;
  RegionEntry regions[MAX_REGION_ENTRIES];
  RegionEntry wildcard;

public:
  RegionMap(TransportKeyStore& store) : _store(&store) {
    next_id = 1; num_regions = 0;
    wildcard.id = wildcard.parent = 0;
    wildcard.flags = REGION_ALLOW_FLOOD;  // default behaviour, allow flood
  }
  void load(FILESYSTEM* _fs);
  void save(FILESYSTEM* _fs);

  RegionEntry* putRegion(const char* name, uint16_t parent_id);
  RegionEntry* findMatch(mesh::Packet* packet, uint8_t mask);
  RegionEntry& getWildcard() { return wildcard; }
  RegionEntry* findByName(const char* name);
  RegionEntry* findById(uint16_t id);
  bool removeRegion(const RegionEntry& region);
  bool clear();
};
