#pragma once

#include <CayenneLPP.h>

#define TELEM_PERM_BASE       0x01   // 'base' permission includes battery
#define TELEM_PERM_LOCATION   0x02

#define TELEM_CHANNEL_SELF   1   // LPP data channel for 'self' device

#define TELEM_INA3221_ADDRESS 0x40      // INA3221 3 channel current, voltage, power sensor I2C address
#define TELEM_INA3221_SHUNT_VALUE 0.100 // most variants will have a 0.1 ohm shunts
#define TELEM_INA3221_SETTING_CH1 "INA3221 Channel 1"
#define TELEM_INA3221_SETTING_CH2 "INA3221 Channel 2"
#define TELEM_INA3221_SETTING_CH3 "INA3221 Channel 3"

class SensorManager {
public:
  double node_lat, node_lon;  // modify these, if you want to affect Advert location

  SensorManager() { node_lat = 0; node_lon = 0; }
  virtual bool begin() { return false; }
  virtual bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) { return false; }
  virtual void loop() { }
  virtual int getNumSettings() const { return 0; }
  virtual const char* getSettingName(int i) const { return NULL; }
  virtual const char* getSettingValue(int i) const { return NULL; }
  virtual bool setSettingValue(const char* name, const char* value) { return false; }
};
