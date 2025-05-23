#pragma once

#include <Mesh.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/LocationProvider.h>


#if ENV_INCLUDE_AHTX0
#define TELEM_AHTX_ADDRESS      0x38      // AHT10, AHT20 temperature and humidity sensor I2C address
#include <Adafruit_AHTX0.h>
#endif
#if ENV_INCLUDE_BME280
#define TELEM_BME280_ADDRESS    0x76      // BME280 environmental sensor I2C address
#define TELEM_BME280_SEALEVELPRESSURE_HPA (1013.25)    // Athmospheric pressure at sea level
#include <Adafruit_BME280.h>
#endif
#if ENV_INCLUDE_INA3221
#define TELEM_INA3221_ADDRESS   0x42      // INA3221 3 channel current sensor I2C address
#define TELEM_INA3221_SHUNT_VALUE 0.100 // most variants will have a 0.1 ohm shunts
#define TELEM_INA3221_NUM_CHANNELS 3
#include <Adafruit_INA3221.h>
#endif
#if ENV_INCLUDE_INA219
#define TELEM_INA219_ADDRESS    0x40      // INA219 single channel current sensor I2C address
#include <Adafruit_INA219.h>
#endif



class EnvironmentSensorManager : public SensorManager {
protected:
  int next_available_channel = TELEM_CHANNEL_SELF + 1;

  bool AHTX0_initialized = false;
  bool BME280_initialized = false;
  bool INA3221_initialized = false;
  bool INA219_initialized = false;
  
  LocationProvider* _location;
  bool gps_detected = false;
  bool gps_active = false;

  #if ENV_INCLUDE_GPS
  void start_gps();
  void stop_gps();
  void initBasicGPS();
  #endif


public:
  EnvironmentSensorManager(LocationProvider &location): _location(&location){};
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;  
  #if ENV_INCLUDE_GPS
  void loop() override;
  #endif
  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
};
