#include <Arduino.h>
#include "target.h"
#include <helpers/ArduinoHelpers.h>

PromicroBoard board;

RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI);

WRAPPER_CLASS radio_driver(radio, board);

VolatileRTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
PromicroSensorManager sensors;

#ifndef LORA_CR
  #define LORA_CR      5
#endif

bool radio_init() {
  rtc_clock.begin(Wire);
  
#ifdef SX126X_DIO3_TCXO_VOLTAGE
  float tcxo = SX126X_DIO3_TCXO_VOLTAGE;
#else
  float tcxo = 1.6f;
#endif

  SPI.setPins(P_LORA_MISO, P_LORA_SCLK, P_LORA_MOSI);
  SPI.begin();
  radio.setRfSwitchPins(SX126X_RXEN, SX126X_TXEN);
  int status = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POWER, 8, tcxo);
  if (status == RADIOLIB_ERR_SPI_CMD_FAILED || status == RADIOLIB_ERR_SPI_CMD_INVALID) {
    #define SX126X_DIO3_TCXO_VOLTAGE (0.0f);
    tcxo = SX126X_DIO3_TCXO_VOLTAGE;
    status = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POWER, 8, tcxo);
  }
  if (status != RADIOLIB_ERR_NONE) {
    Serial.print("ERROR: radio init failed: ");
    Serial.println(status);
    return false;  // fail
  }
  
  radio.setCRC(1);
  
#ifdef SX126X_CURRENT_LIMIT
  radio.setCurrentLimit(SX126X_CURRENT_LIMIT);
#endif
#ifdef SX126X_DIO2_AS_RF_SWITCH
  radio.setDio2AsRfSwitch(SX126X_DIO2_AS_RF_SWITCH);
#endif
#ifdef SX126X_RX_BOOSTED_GAIN
  radio.setRxBoostedGainMode(SX126X_RX_BOOSTED_GAIN);
#endif

  return true;  // success
}

uint32_t radio_get_rng_seed() {
  return radio.random(0x7FFFFFFF);
}

void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
  radio.setFrequency(freq);
  radio.setSpreadingFactor(sf);
  radio.setBandwidth(bw);
  radio.setCodingRate(cr);
}

void radio_set_tx_power(uint8_t dbm) {
  radio.setOutputPower(dbm);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}

static INA3221 INA_3221(TELEM_INA3221_ADDRESS, &Wire);
static INA219 INA_219(TELEM_INA219_ADDRESS, &Wire);

bool PromicroSensorManager::begin() {
  if (INA_3221.begin()) {
    MESH_DEBUG_PRINTLN("Found INA3221 at address: %02X", INA_3221.getAddress());
    MESH_DEBUG_PRINTLN("%04X %04X %04X", INA_3221.getDieID(), INA_3221.getManufacturerID(), INA_3221.getConfiguration());

    for(int i = 0; i < 3; i++) {
      INA_3221.setShuntR(i, TELEM_INA3221_SHUNT_VALUE);
    }
    INA3221initialized = true;
  } else {
    INA3221initialized = false;
    MESH_DEBUG_PRINTLN("INA3221 was not found at I2C address %02X", TELEM_INA3221_ADDRESS);
  }
  if (INA_219.begin()) {
    MESH_DEBUG_PRINTLN("Found INA219 at address: %02X", INA_219.getAddress());
    INA219_CHANNEL = INA3221initialized ? TELEM_CHANNEL_SELF + 4 : TELEM_CHANNEL_SELF + 1;
    INA_219.setMaxCurrentShunt(TELEM_INA219_MAX_CURRENT, TELEM_INA219_SHUNT_VALUE);
    INA219initialized = true;
  } else {
    INA219initialized = false;
    MESH_DEBUG_PRINTLN("INA219 was not found at I2C address %02X", TELEM_INA219_ADDRESS);
  }
  return true;
}

bool PromicroSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  if (requester_permissions & TELEM_PERM_ENVIRONMENT) {
    if (INA3221initialized) {
      for(int i = 0; i < 3; i++) {
        // add only enabled INA3221 channels to telemetry
        if (INA3221_CHANNEL_ENABLED[i]) {
          telemetry.addVoltage(INA3221_CHANNELS[i], INA_3221.getBusVoltage(i));
          telemetry.addCurrent(INA3221_CHANNELS[i], INA_3221.getCurrent(i));
          telemetry.addPower(INA3221_CHANNELS[i], INA_3221.getPower(i));
        }
      }
    }
    if(INA219initialized) {
      telemetry.addVoltage(INA219_CHANNEL, INA_219.getBusVoltage());
      telemetry.addCurrent(INA219_CHANNEL, INA_219.getCurrent());
      telemetry.addPower(INA219_CHANNEL, INA_219.getPower());
    }
  }

  return true;
}

int PromicroSensorManager::getNumSettings() const {
  return NUM_SENSOR_SETTINGS;
}

const char* PromicroSensorManager::getSettingName(int i) const {
  if (i >= 0 && i < NUM_SENSOR_SETTINGS) {
    return INA3221_CHANNEL_NAMES[i];
  }
  return NULL;
}

const char* PromicroSensorManager::getSettingValue(int i) const {
  if (i >= 0 && i < NUM_SENSOR_SETTINGS) {
    return INA3221_CHANNEL_ENABLED[i] ? "1" : "0";
  }
  return NULL;
}

bool PromicroSensorManager::setSettingValue(const char* name, const char* value) {
  for (int i = 0; i < NUM_SENSOR_SETTINGS; i++) {
    if (strcmp(name, INA3221_CHANNEL_NAMES[i]) == 0) {
      int channelEnabled = INA_3221.getEnableChannel(i);
      if (strcmp(value, "1") == 0) {
        INA3221_CHANNEL_ENABLED[i] = true;
        if (!channelEnabled) {
          INA_3221.enableChannel(i);
        }
      } else {
        INA3221_CHANNEL_ENABLED[i] = false;
        if (channelEnabled) {
          INA_3221.disableChannel(i);
        }
      }
      return true;
    }
  }
  return false;
}