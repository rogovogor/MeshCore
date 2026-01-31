#include <Arduino.h>
#include <target.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/IdentityStore.h>
#include <CayenneLPP.h>
#include "KissModem.h"

#if defined(NRF52_PLATFORM)
  #include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
#elif defined(ESP32)
  #include <SPIFFS.h>
#endif

StdRNG rng;
mesh::LocalIdentity identity;
KissModem* modem;

void halt() {
  while (1) ;
}

void loadOrCreateIdentity() {
#if defined(NRF52_PLATFORM)
  InternalFS.begin();
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "Filesystem not defined"
#endif

  if (!store.load("_main", identity)) {
    identity = radio_new_identity();
    while (identity.pub_key[0] == 0x00 || identity.pub_key[0] == 0xFF) {
      identity = radio_new_identity();
    }
    store.save("_main", identity);
  }
}

void onSetRadio(float freq, float bw, uint8_t sf, uint8_t cr) {
  radio_set_params(freq, bw, sf, cr);
}

void onSetTxPower(uint8_t power) {
  radio_set_tx_power(power);
}

void onSetSyncWord(uint8_t sync_word) {
  radio_set_sync_word(sync_word);
}

float onGetCurrentRssi() {
  return radio_driver.getCurrentRSSI();
}

bool onIsChannelBusy() {
  return radio_driver.isReceiving();
}

uint32_t onGetAirtime(uint8_t len) {
  return radio_driver.getEstAirtimeFor(len);
}

int16_t onGetNoiseFloor() {
  return radio_driver.getNoiseFloor();
}

void onGetStats(uint32_t* rx, uint32_t* tx, uint32_t* errors) {
  *rx = radio_driver.getPacketsRecv();
  *tx = radio_driver.getPacketsSent();
  *errors = radio_driver.getPacketsRecvErrors();
}

uint16_t onGetBattery() {
  return board.getBattMilliVolts();
}

uint8_t onGetSensors(uint8_t permissions, uint8_t* buffer, uint8_t max_len) {
  CayenneLPP telemetry(max_len);
  if (sensors.querySensors(permissions, telemetry)) {
    uint8_t len = telemetry.getSize();
    memcpy(buffer, telemetry.getBuffer(), len);
    return len;
  }
  return 0;
}

void setup() {
  board.begin();

  if (!radio_init()) {
    halt();
  }

  radio_driver.begin();

  rng.begin(radio_get_rng_seed());
  loadOrCreateIdentity();

  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) delay(10);
  delay(100);

  sensors.begin();

  modem = new KissModem(Serial, identity, rng);
  modem->setRadioCallback(onSetRadio);
  modem->setTxPowerCallback(onSetTxPower);
  modem->setSyncWordCallback(onSetSyncWord);
  modem->setGetCurrentRssiCallback(onGetCurrentRssi);
  modem->setIsChannelBusyCallback(onIsChannelBusy);
  modem->setGetAirtimeCallback(onGetAirtime);
  modem->setGetNoiseFloorCallback(onGetNoiseFloor);
  modem->setGetStatsCallback(onGetStats);
  modem->setGetBatteryCallback(onGetBattery);
  modem->setGetSensorsCallback(onGetSensors);
  modem->begin();
}

void loop() {
  modem->loop();

  uint8_t packet[KISS_MAX_PACKET_SIZE];
  uint16_t len;
  
  if (modem->getPacketToSend(packet, &len)) {
    radio_driver.startSendRaw(packet, len);
    while (!radio_driver.isSendComplete()) {
      delay(1);
    }
    radio_driver.onSendFinished();
    modem->onTxComplete(true);
  }

  uint8_t rx_buf[256];
  int rx_len = radio_driver.recvRaw(rx_buf, sizeof(rx_buf));
  
  if (rx_len > 0) {
    int8_t snr = (int8_t)(radio_driver.getLastSNR() * 4);
    int8_t rssi = (int8_t)radio_driver.getLastRSSI();
    modem->onPacketReceived(snr, rssi, rx_buf, rx_len);
  }

  radio_driver.loop();
}
