#pragma once

#include <Mesh.h>
#include <INA3221.h>

#define TELEM_INA3221_ADDRESS 0x42      // INA3221 3 channel current sensor I2C address
#define TELEM_INA3221_SHUNT_VALUE 0.100 // most variants will have a 0.1 ohm shunts

#define NUM_CHANNELS 3

static INA3221 INA_3221(TELEM_INA3221_ADDRESS, &Wire);

class INA3221Sensor {
    bool initialized = false;

public:
    void begin() {
        if (INA_3221.begin()) {
            MESH_DEBUG_PRINTLN("Found INA3221 at address: %02X", INA_3221.getAddress());
            MESH_DEBUG_PRINTLN("%04X %04X %04X", INA_3221.getDieID(), INA_3221.getManufacturerID(), INA_3221.getConfiguration());

            for(int i = 0; i < 3; i++) {
            INA_3221.setShuntR(i, TELEM_INA3221_SHUNT_VALUE);
            }
            initialized = true;
        } else {
            initialized = false;
            MESH_DEBUG_PRINTLN("INA3221 was not found at I2C address %02X", TELEM_INA3221_ADDRESS);
        }
    }

    bool isInitialized() const { return initialized; }

    int numChannels() const { return NUM_CHANNELS; }

    float getVoltage(int channel) const {
        if (initialized && channel >=0 && channel < NUM_CHANNELS) {
            return INA_3221.getBusVoltage(channel);
        }
        return 0;
    }

    float getCurrent(int channel) const {
        if (initialized && channel >=0 && channel < NUM_CHANNELS) {
            return INA_3221.getCurrent(channel);
        }
        return 0;
    }

    float getPower (int channel) const {
        if (initialized && channel >=0 && channel < NUM_CHANNELS) {
            return INA_3221.getPower(channel);
        }
        return 0;
    }

    bool setChannelEnabled(int channel, bool enabled) {
        if (initialized && channel >=0 && channel < NUM_CHANNELS) {
            INA_3221.enableChannel(channel);
            return true;
        }
        return false;
    }

    bool getChannelEnabled(int channel) const {
        if (initialized && channel >=0 && channel < NUM_CHANNELS) {
            return INA_3221.getEnableChannel(channel);
        }
        return false;
    }
};
