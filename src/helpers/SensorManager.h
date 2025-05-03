#pragma once

#include <CayenneLPP.h>

#define TELEM_PERM_LOCATION   0x02

#define TELEM_CHANNEL_SELF   1   // LPP data channel for 'self' device

class SensorManager {
public:
  virtual bool begin() { return false; }
  virtual bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) { return false; }
  virtual void loop() { }
};
