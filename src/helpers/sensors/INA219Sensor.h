#pragma once

#include <Mesh.h>
#include <INA219.h>

#define TELEM_INA219_ADDRESS  0x40      // INA219 single channel current sensor I2C address
#define TELEM_INA219_SHUNT_VALUE 0.100  // shunt value in ohms (may differ between manufacturers)
#define TELEM_INA219_MAX_CURRENT 5

static INA219 INA_219(TELEM_INA219_ADDRESS, &Wire);

class INA219Sensor {
    bool initialized = false;
public:    
    void begin() {
        if (INA_219.begin()) {
            MESH_DEBUG_PRINTLN("Found INA219 at address: %02X", INA_219.getAddress());
            INA_219.setMaxCurrentShunt(TELEM_INA219_MAX_CURRENT, TELEM_INA219_SHUNT_VALUE);
            initialized = true;
        } else {
            initialized = false;
            MESH_DEBUG_PRINTLN("INA219 was not found at I2C address %02X", TELEM_INA219_ADDRESS);
        }
    }

    bool isInitialized() const { return initialized; }

    float getVoltage() const {
        if (initialized) {
            return INA_219.getBusVoltage();    
        }
        return 0;
    }

    float getCurrent() const {
        if (initialized) {
            return INA_219.getCurrent();    
        }
        return 0;
    }

    float getPower() const {
        if (initialized) {
            return INA_219.getPower();    
        }
        return 0;
    }
};
