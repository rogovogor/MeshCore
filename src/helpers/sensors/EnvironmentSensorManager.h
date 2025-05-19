#pragma once

#include <helpers/SensorManager.h>
#include "INA3221Sensor.h"
#include "INA219Sensor.h"
#include "AHTX0Sensor.h"
#include "BME280Sensor.h"

#define NUM_SENSOR_SETTINGS 3

#define TELEM_INA3221_SETTING_CH1 "INA3221-1"
#define TELEM_INA3221_SETTING_CH2 "INA3221-2"
#define TELEM_INA3221_SETTING_CH3 "INA3221-3"

class EnvironmentSensorManager : public SensorManager {
// INA3221 channels in telemetry
const char * INA3221_CHANNEL_NAMES[NUM_SENSOR_SETTINGS] = { TELEM_INA3221_SETTING_CH1, TELEM_INA3221_SETTING_CH2, TELEM_INA3221_SETTING_CH3};

protected:
  int next_available_channel = TELEM_CHANNEL_SELF + 1;
  INA3221Sensor INA3221_sensor;
  AHTX0Sensor AHTX0_sensor;
  INA219Sensor INA219_sensor;
  BME280Sensor BME280_sensor;
public:
  EnvironmentSensorManager(){};
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
};
