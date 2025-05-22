#pragma once

#include <helpers/SensorManager.h>
#include <helpers/sensors/LocationProvider.h>
#include <Mesh.h>
#include <Adafruit_INA3221.h>
#include <Adafruit_INA219.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BME280.h>

#define TELEM_AHTX_ADDRESS      0x38      // AHT10, AHT20 temperature and humidity sensor I2C address
#define TELEM_BME280_ADDRESS    0x76      // BME280 environmental sensor I2C address
#define TELEM_INA3221_ADDRESS   0x42      // INA3221 3 channel current sensor I2C address
#define TELEM_INA219_ADDRESS    0x40      // INA219 single channel current sensor I2C address

#define TELEM_INA3221_SHUNT_VALUE 0.100 // most variants will have a 0.1 ohm shunts
#define TELEM_INA3221_NUM_CHANNELS 3

#define TELEM_BME280_SEALEVELPRESSURE_HPA (1013.25)    // Athmospheric pressure at sea level

class EnvironmentSensorManager : public SensorManager {
protected:
  int next_available_channel = TELEM_CHANNEL_SELF + 1;

  bool INA3221_initialized = false;
  bool INA219_initialized = false;
  bool BME280_initialized = false;
  bool AHTX0_initialized = false;
  
  bool gps_active = false;
  bool gps_detected = false;
  LocationProvider* _location;

  void start_gps();
  void stop_gps();
  void initSerialGPS();

public:
  EnvironmentSensorManager(LocationProvider &location): _location(&location){};
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;  
  void loop() override;
  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
};
