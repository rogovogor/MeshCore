#include "RegionMap.h"
#include <helpers/TxtDataHelpers.h>
#include <SHA256.h>

void RegionMap::load(FILESYSTEM* _fs) {
  // TODO
}
void RegionMap::save(FILESYSTEM* _fs) {
  // TODO
}

RegionEntry* RegionMap::putRegion(const char* name, uint16_t parent_id) {
  auto region = findByName(name);
  if (region) {
    if (region->id == parent_id) return NULL;   // ERROR: invalid parent!

    region->parent = parent_id;   // re-parent / move this region in the hierarchy
  } else {
    if (num_regions >= MAX_REGION_ENTRIES) return NULL;  // full!

    region = &regions[num_regions++];   // alloc new RegionEntry
    region->flags = 0;
    region->id = next_id++;
    StrHelper::strncpy(region->name, name, sizeof(region->name));
    region->parent = parent_id;
  }
  return region;
}

RegionEntry* RegionMap::findMatch(mesh::Packet* packet, uint8_t mask) {
  for (int i = 0; i < num_regions; i++) {
    auto region = &regions[i];
    if ((region->flags & mask) == mask) {   // does region allow this? (per 'mask' param)
      TransportKey keys[4];
      int num;
      if (region->name[0] == '#') {   // auto hashtag region
        _store->getAutoKeyFor(region->id, region->name, keys[0]);
        num = 1;
      } else {
        num = _store->loadKeysFor(region->id, keys, 4);
      }
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

RegionEntry* RegionMap::findByName(const char* name) {
  for (int i = 0; i < num_regions; i++) {
    auto region = &regions[i];
    if (strcmp(name, region->name) == 0) return region;
  }
  return NULL;  // not found
}

RegionEntry* RegionMap::findById(uint16_t id) {
  if (id == 0) return &wildcard;   // special root Region

  for (int i = 0; i < num_regions; i++) {
    auto region = &regions[i];
    if (region->id == id) return region;
  }
  return NULL;  // not found
}

bool RegionMap::removeRegion(const RegionEntry& region) {
  if (region.id == 0) return false;  // failed (cannot remove the wildcard Region)

  int i;     // first check region has no child regions
  for (i = 0; i < num_regions; i++) {
    if (regions[i].parent == region.id) return false;   // failed (must remove child Regions first)
  }

  i = 0;
  while (i < num_regions) {
    if (region.id == regions[i].id) break;
    i++;
  }
  if (i >= num_regions) return false;  // failed (not found)

  num_regions--;    // remove from regions array
  while (i + 1 < num_regions) {
    regions[i] = regions[i + 1];
  }
  return true;  // success
}

bool RegionMap::clear() {
  num_regions = 0;
  return true;  // success
}
