#include "RegionMap.h"
#include <SHA256.h>

void RegionMap::load(FILESYSTEM* _fs) {
  // TODO
}
void RegionMap::save(FILESYSTEM* _fs) {
  // TODO
}

RegionEntry* RegionMap::findMatch(mesh::Packet* packet, uint8_t mask) {
  for (int i = 0; i < num_regions; i++) {
    auto region = &regions[i];
    if (region->flags & mask) {   // does region allow this? (per 'mask' param)
      TransportKey keys[4];
      int num = _store->loadKeysFor(region->name, region->id, keys, 4);
      for (int j = 0; j < num; j++) {
        uint16_t code = keys[j].calcTransportCode(packet);
        if (packet->transport_codes[0] == code) {   // a match!!
          return region;
        }
      }
    }
  }
  return NULL;  // no matches
}

const RegionEntry* RegionMap::findName(const char* name) const {
  for (int i = 0; i < num_regions; i++) {
    auto region = &regions[i];
    if (strcmp(name, region->name) == 0) return region;
  }
  return NULL;  // not found
}

