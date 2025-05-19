#include "EnvironmentSensorManager.h"

bool EnvironmentSensorManager::begin() {
  INA3221_sensor.begin();
  INA219_sensor.begin();
  AHTX0_sensor.begin();
  BME280_sensor.begin();

  return true;
}

bool EnvironmentSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  next_available_channel = TELEM_CHANNEL_SELF + 1;
  if (requester_permissions & TELEM_PERM_ENVIRONMENT) {
    if (INA3221_sensor.isInitialized()) {
      for(int i = 0; i < 3; i++) {
        // add only enabled INA3221 channels to telemetry
        if (INA3221_sensor.getChannelEnabled(i)) {
          telemetry.addVoltage(next_available_channel, INA3221_sensor.getVoltage(i));
          telemetry.addCurrent(next_available_channel, INA3221_sensor.getCurrent(i));
          telemetry.addPower(next_available_channel, INA3221_sensor.getPower(i));
          next_available_channel++;
        }
      }
    }
    if (INA219_sensor.isInitialized()) {
        telemetry.addVoltage(next_available_channel, INA219_sensor.getVoltage());
        telemetry.addCurrent(next_available_channel, INA219_sensor.getCurrent());
        telemetry.addPower(next_available_channel, INA219_sensor.getPower());
        next_available_channel++;
    }
    if (AHTX0_sensor.isInitialized()) {
        telemetry.addTemperature(TELEM_CHANNEL_SELF, AHTX0_sensor.getTemperature());
        telemetry.addRelativeHumidity(TELEM_CHANNEL_SELF, AHTX0_sensor.getRelativeHumidity());
    }
    if (BME280_sensor.isInitialized()) {
        telemetry.addTemperature(TELEM_CHANNEL_SELF, BME280_sensor.getTemperature());
        telemetry.addRelativeHumidity(TELEM_CHANNEL_SELF, BME280_sensor.getRelativeHumidity());
        telemetry.addBarometricPressure(TELEM_CHANNEL_SELF, BME280_sensor.getBarometricPressure());
        telemetry.addAltitude(TELEM_CHANNEL_SELF, BME280_sensor.getAltitude());
    }
  }

  return true;
}

int EnvironmentSensorManager::getNumSettings() const {
  return NUM_SENSOR_SETTINGS;
}

const char* EnvironmentSensorManager::getSettingName(int i) const {
  if (i >= 0 && i < NUM_SENSOR_SETTINGS) {
    return INA3221_CHANNEL_NAMES[i];
  }
  return NULL;
}

const char* EnvironmentSensorManager::getSettingValue(int i) const {
  if (i >= 0 && i < NUM_SENSOR_SETTINGS) {
    return INA3221_sensor.getChannelEnabled(i) == true ? "1" : "0";
  }
  return NULL;
}

bool EnvironmentSensorManager::setSettingValue(const char* name, const char* value) {
  for (int i = 0; i < NUM_SENSOR_SETTINGS; i++) {
    if (strcmp(name, INA3221_CHANNEL_NAMES[i]) == 0) {
        bool channel_enabled = strcmp(value, "1") == 0 ? true : false;
        INA3221_sensor.setChannelEnabled(i, channel_enabled);
        return true;
    }
  }
  return false;
}