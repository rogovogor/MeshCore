#ifndef NODE_PREFS_H
#define NODE_PREFS_H

#include <cstdint> // For uint8_t, uint32_t

struct NodePrefs {  // persisted to file
  float airtime_factor;
  char node_name[32];
  double node_lat, node_lon;
  float freq;
  uint8_t sf;
  uint8_t cr;
  uint8_t reserved1;
  uint8_t manual_add_contacts;
  float bw;
  uint8_t tx_power_dbm;
  uint8_t unused[3];
  float rx_delay_base;
  uint32_t ble_pin;
};

#endif // NODE_PREFS_H