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

PromicroSensorManager::PromicroSensorManager(){
  INA_3221 = new INA3221(TELEM_INA3221_ADDRESS, &Wire);
}

PromicroSensorManager::~PromicroSensorManager(){
  if (INA_3221) {
    delete INA_3221;
    INA_3221 = nullptr;
  }
}

bool PromicroSensorManager::begin() {
  if (INA_3221->begin() ) {
    Serial.print("Found INA3221 at address ");
    Serial.print(INA_3221->getAddress());
    Serial.println();
    Serial.print(INA_3221->getDieID(), HEX);
    Serial.print(INA_3221->getManufacturerID(), HEX);
    Serial.print(INA_3221->getConfiguration(), HEX);
    Serial.println();

    for(int i = 0; i < 3; i++) {
      INA_3221->setShuntR(i, TELEM_INA3221_SHUNT_VALUE);
    }
    // add INA3221 settings to settings manager
    settingsManager.addSetting(TELEM_INA3221_SETTING_CH1, true);
    settingsManager.addSetting(TELEM_INA3221_SETTING_CH2, true);
    settingsManager.addSetting(TELEM_INA3221_SETTING_CH3, true);
    INA3221initialized = true;
  }
  else {
    INA3221initialized = false;
    Serial.print("INA3221 was not found at I2C address ");
    Serial.print(TELEM_INA3221_ADDRESS, HEX);
    Serial.println();
  }
  return true;
}

bool PromicroSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  // TODO: what is the correct permission here?
  if (requester_permissions && TELEM_PERM_BASE) {
    if (INA3221initialized) {
      for(int i = 0; i < 3; i++) {
        // add only enabled INA3221 channels to telemetry
        if (settingsManager.getSettingValue(INA3221_CHANNEL_NAMES[i]) == "true") {
          
          // TODO: remove when telemetry support gets properly added
          // Serial.print("CH");
          // Serial.print(i);
          // Serial.print(" Voltage: ");
          // Serial.print(INA_3221->getBusVoltage(i));
          // Serial.print("V Current: ");
          // Serial.print(INA_3221->getCurrent(i));
          // Serial.print("A Power: ");
          // Serial.print(INA_3221->getPower(i));
          // Serial.println();

          telemetry.addVoltage(INA3221_CHANNELS[i], INA_3221->getBusVoltage(i));
          telemetry.addCurrent(INA3221_CHANNELS[i], INA_3221->getCurrent(i));
          telemetry.addPower(INA3221_CHANNELS[i], INA_3221->getPower(i));
        }
      }
    }
  }

  return true;
}

int PromicroSensorManager::getNumSettings() const {
  return settingsManager.getSettingCount();
}

const char* PromicroSensorManager::getSettingName(int i) const {
  return settingsManager.getSettingName(i);
}

const char* PromicroSensorManager::getSettingValue(int i) const {
  return settingsManager.getSettingValue(i);
}

bool PromicroSensorManager::setSettingValue(const char* name, const char* value) {
  if (settingsManager.setSettingValue(name, value)) {
    onSettingsChanged();
    return true;
  }
  return false;
}

void PromicroSensorManager::onSettingsChanged() {
  if (INA3221initialized) {
    for(int i = 0; i < 3; i++) {
      int channelEnabled = INA_3221->getEnableChannel(i);
      bool settingEnabled = settingsManager.getSettingValue(INA3221_CHANNEL_NAMES[i]) == "true";
      if (!settingEnabled && channelEnabled) {
        INA_3221->disableChannel(i);
      }
      if (settingEnabled && !channelEnabled) {
        INA_3221->enableChannel(i);
      }
    }
  }
}