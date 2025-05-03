#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/RadioLibWrappers.h>
#include <helpers/nrf52/T1000eBoard.h>
#include <helpers/CustomLR1110Wrapper.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/SensorManager.h>

class T1000SensorManager : public SensorManager {
  float _lat, _lon, _alt;
public:
  T1000SensorManager(): _lat(0), _lon(0), _alt(0) { }
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  void loop() override;
};

extern T1000eBoard board;
extern WRAPPER_CLASS radio_driver;
extern VolatileRTCClock rtc_clock;
extern T1000SensorManager sensors;

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(uint8_t dbm);
mesh::LocalIdentity radio_new_identity();
