#include <Arduino.h>
#include <target.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/IdentityStore.h>
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

float onGetCurrentRssi() {
  return radio_driver.getCurrentRSSI();
}

void onGetStats(uint32_t* rx, uint32_t* tx, uint32_t* errors) {
  *rx = radio_driver.getPacketsRecv();
  *tx = radio_driver.getPacketsSent();
  *errors = radio_driver.getPacketsRecvErrors();
}

void onSendPacket(const uint8_t* data, uint16_t len) {
  radio_driver.startSendRaw(data, len);
}

bool onIsSendComplete() {
  return radio_driver.isSendComplete();
}

void onSendFinished() {
  radio_driver.onSendFinished();
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

  modem = new KissModem(Serial, identity, rng, radio_driver, board, sensors);
  modem->setRadioCallback(onSetRadio);
  modem->setTxPowerCallback(onSetTxPower);
  modem->setGetCurrentRssiCallback(onGetCurrentRssi);
  modem->setGetStatsCallback(onGetStats);
  modem->setSendPacketCallback(onSendPacket);
  modem->setIsSendCompleteCallback(onIsSendComplete);
  modem->setOnSendFinishedCallback(onSendFinished);
  modem->begin();
}

void loop() {
  modem->loop();

  if (!modem->isTxBusy()) {
    uint8_t rx_buf[256];
    int rx_len = radio_driver.recvRaw(rx_buf, sizeof(rx_buf));

    if (rx_len > 0) {
      int8_t snr = (int8_t)(radio_driver.getLastSNR() * 4);
      int8_t rssi = (int8_t)radio_driver.getLastRSSI();
      modem->onPacketReceived(snr, rssi, rx_buf, rx_len);
    }
  }

  radio_driver.loop();
}
