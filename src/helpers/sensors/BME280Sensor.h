#pragma once
#include <Mesh.h>
#include <Adafruit_BME280.h>

#define TELEM_BME280_ADDRESS    0x76      // BME280 environmental sensor I2C address
#define SEALEVELPRESSURE_HPA (1013.25)    // Athmospheric pressure at sea level

static Adafruit_BME280 BME280;

class BME280Sensor {
    bool initialized = false;
public:    
    void begin() {
        if (BME280.begin(TELEM_BME280_ADDRESS, &Wire)) {
            MESH_DEBUG_PRINTLN("Found BME280 at address: %02X", TELEM_BME280_ADDRESS);
            MESH_DEBUG_PRINTLN("BME sensor ID: %02X", BME280.sensorID);
            initialized = true;
        } else {
            initialized = false;
            MESH_DEBUG_PRINTLN("BME280 was not found at I2C address %02X", TELEM_BME280_ADDRESS);
        }
    }

    bool isInitialized() const { return initialized; };

    float getRelativeHumidity() const {
        if (initialized) {
            return BME280.readHumidity();;
        }
    }

    float getTemperature() const {
        if (initialized) {
            return BME280.readTemperature();;
        }
    }

    float getBarometricPressure() const {
        if (initialized) {
            return BME280.readPressure();
        }
    }

    float getAltitude() const {
        if (initialized) {
            return BME280.readAltitude(SEALEVELPRESSURE_HPA);
        }
    }

    void setTemperatureCompensation(float delta) {
        if (initialized) {
            BME280.setTemperatureCompensation(delta);
        }
    }
};
