#include "EnvironmentSensorManager.h"

static Adafruit_AHTX0 AHTX0;
static Adafruit_BME280 BME280;
static Adafruit_INA3221 INA3221;
static Adafruit_INA219 INA219(TELEM_INA219_ADDRESS);

bool EnvironmentSensorManager::begin() {

  if (INA3221.begin(TELEM_INA3221_ADDRESS, &Wire)) {
      MESH_DEBUG_PRINTLN("Found INA3221 at address: %02X", TELEM_INA3221_ADDRESS);
      MESH_DEBUG_PRINTLN("%04X %04X", INA3221.getDieID(), INA3221.getManufacturerID());

      for(int i = 0; i < 3; i++) {
        INA3221.setShuntResistance(i, TELEM_INA3221_SHUNT_VALUE);
      }
      INA3221_initialized = true;
  } else {
      INA3221_initialized = false;
      MESH_DEBUG_PRINTLN("INA3221 was not found at I2C address %02X", TELEM_INA3221_ADDRESS);
  }

  if (INA219.begin(&Wire)) {
      MESH_DEBUG_PRINTLN("Found INA219 at address: %02X", TELEM_INA219_ADDRESS);
      INA219_initialized = true;
  } else {
      INA219_initialized = false;
      MESH_DEBUG_PRINTLN("INA219 was not found at I2C address %02X", TELEM_INA219_ADDRESS);
  }

  if (AHTX0.begin(&Wire, 0, TELEM_AHTX_ADDRESS)) {
      MESH_DEBUG_PRINTLN("Found AHT10/AHT20 at address: %02X", TELEM_AHTX_ADDRESS);
      AHTX0_initialized = true;
  } else {
      AHTX0_initialized = false;
      MESH_DEBUG_PRINTLN("AHT10/AHT20 was not found at I2C address %02X", TELEM_AHTX_ADDRESS);
  }

  if (BME280.begin(TELEM_BME280_ADDRESS, &Wire)) {
      MESH_DEBUG_PRINTLN("Found BME280 at address: %02X", TELEM_BME280_ADDRESS);
      MESH_DEBUG_PRINTLN("BME sensor ID: %02X", BME280.sensorID);
      BME280_initialized = true;
  } else {
      BME280_initialized = false;
      MESH_DEBUG_PRINTLN("BME280 was not found at I2C address %02X", TELEM_BME280_ADDRESS);
  }

  return true;
}

bool EnvironmentSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  next_available_channel = TELEM_CHANNEL_SELF + 1;
  if (requester_permissions & TELEM_PERM_ENVIRONMENT) {
    if (INA3221_initialized) {
      for(int i = 0; i < TELEM_INA3221_NUM_CHANNELS; i++) {
        // add only enabled INA3221 channels to telemetry
        if (INA3221.isChannelEnabled(i)) {
          float voltage = INA3221.getBusVoltage(i);
          float current = INA3221.getCurrentAmps(i);
          telemetry.addVoltage(next_available_channel, voltage);
          telemetry.addCurrent(next_available_channel, current);
          telemetry.addPower(next_available_channel, voltage * current);
          next_available_channel++;
        }
      }
    }
    if (INA219_initialized) {
        telemetry.addVoltage(next_available_channel, INA219.getBusVoltage_V());
        telemetry.addCurrent(next_available_channel, INA219.getCurrent_mA() / 1000);
        telemetry.addPower(next_available_channel, INA219.getPower_mW() / 1000);
        next_available_channel++;
    }
    if (AHTX0_initialized) {
      sensors_event_t humidity, temp;
      AHTX0.getEvent(&humidity, &temp);

      telemetry.addTemperature(TELEM_CHANNEL_SELF, temp.temperature);
      telemetry.addRelativeHumidity(TELEM_CHANNEL_SELF, humidity.relative_humidity);      
    }

    if (BME280_initialized) {
        telemetry.addTemperature(TELEM_CHANNEL_SELF, BME280.readTemperature());
        telemetry.addRelativeHumidity(TELEM_CHANNEL_SELF, BME280.readHumidity());
        telemetry.addBarometricPressure(TELEM_CHANNEL_SELF, BME280.readPressure());
        telemetry.addAltitude(TELEM_CHANNEL_SELF, BME280.readAltitude(TELEM_BME280_SEALEVELPRESSURE_HPA));
    }
  }

  return true;
}