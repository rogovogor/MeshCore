#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/RadioLibWrappers.h>
#include <helpers/nrf52/PromicroBoard.h>
#include <helpers/CustomSX1262Wrapper.h>
#include <helpers/CustomLLCC68Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/SensorSettingsManager.h>
#include <INA3221.h>

extern PromicroBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;


bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(uint8_t dbm);
mesh::LocalIdentity radio_new_identity();

class PromicroSensorManager: public SensorManager {
  INA3221 * INA_3221;
  bool INA3221initialized = false;
  SensorSettingsManager settingsManager;

  // INA3221 channels in telemetry
  int INA3221_CHANNELS[3] = {TELEM_CHANNEL_SELF + 1, TELEM_CHANNEL_SELF + 2, TELEM_CHANNEL_SELF+ 3};
  char * INA3221_CHANNEL_NAMES[3] = { TELEM_INA3221_SETTING_CH1, TELEM_INA3221_SETTING_CH2, TELEM_INA3221_SETTING_CH3};
  void onSettingsChanged();

public:
  PromicroSensorManager();
  ~PromicroSensorManager();
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
};


extern PromicroSensorManager sensors;