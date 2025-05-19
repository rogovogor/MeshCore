#pragma once
#include <Mesh.h>
#include <Adafruit_AHTX0.h>

#define TELEM_AHTX_ADDRESS    0x38      // AHT10, AHT20 temperature and humidity sensor I2C address

static Adafruit_AHTX0 AHTX0;

class AHTX0Sensor {
    bool initialized = false;
public:    
    void begin() {
        if (AHTX0.begin(&Wire, 0, TELEM_AHTX_ADDRESS)) {
            MESH_DEBUG_PRINTLN("Found AHT10/AHT20 at address: %02X", TELEM_AHTX_ADDRESS);
            initialized = true;
        } else {
            initialized = false;
            MESH_DEBUG_PRINTLN("AHT10/AHT20 was not found at I2C address %02X", TELEM_AHTX_ADDRESS);
        }
    }

    bool isInitialized() const { return initialized; };

    float getRelativeHumidity() const {
        if (initialized) {
            sensors_event_t humidity, temp;
            AHTX0.getEvent(&humidity, &temp);
            return humidity.relative_humidity;
        }
    }

    float getTemperature() const {
        if (initialized) {
            sensors_event_t humidity, temp;
            AHTX0.getEvent(&humidity, &temp);
            return temp.temperature;
        }
    }
};
