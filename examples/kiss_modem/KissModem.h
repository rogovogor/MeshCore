#pragma once

#include <Arduino.h>
#include <Identity.h>
#include <Utils.h>

#define KISS_FEND  0xC0
#define KISS_FESC  0xDB
#define KISS_TFEND 0xDC
#define KISS_TFESC 0xDD

#define KISS_MAX_FRAME_SIZE 512
#define KISS_MAX_PACKET_SIZE 255

#define CMD_DATA              0x00
#define CMD_GET_IDENTITY      0x01
#define CMD_GET_RANDOM        0x02
#define CMD_VERIFY_SIGNATURE  0x03
#define CMD_SIGN_DATA         0x04
#define CMD_ENCRYPT_DATA      0x05
#define CMD_DECRYPT_DATA      0x06
#define CMD_KEY_EXCHANGE      0x07
#define CMD_HASH              0x08
#define CMD_SET_RADIO         0x09
#define CMD_SET_TX_POWER      0x0A
#define CMD_SET_SYNC_WORD     0x0B
#define CMD_GET_RADIO         0x0C
#define CMD_GET_TX_POWER      0x0D
#define CMD_GET_SYNC_WORD     0x0E
#define CMD_GET_VERSION       0x0F

#define RESP_IDENTITY         0x11
#define RESP_RANDOM           0x12
#define RESP_VERIFY           0x13
#define RESP_SIGNATURE        0x14
#define RESP_ENCRYPTED        0x15
#define RESP_DECRYPTED        0x16
#define RESP_SHARED_SECRET    0x17
#define RESP_HASH             0x18
#define RESP_OK               0x19
#define RESP_RADIO            0x1A
#define RESP_TX_POWER         0x1B
#define RESP_SYNC_WORD        0x1C
#define RESP_VERSION          0x1D
#define RESP_ERROR            0x1E
#define RESP_TX_DONE          0x1F

#define ERR_INVALID_LENGTH    0x01
#define ERR_INVALID_PARAM     0x02
#define ERR_NO_CALLBACK       0x03
#define ERR_MAC_FAILED        0x04
#define ERR_UNKNOWN_CMD       0x05
#define ERR_ENCRYPT_FAILED    0x06

#define KISS_FIRMWARE_VERSION 1

typedef void (*SetRadioCallback)(float freq, float bw, uint8_t sf, uint8_t cr);
typedef void (*SetTxPowerCallback)(uint8_t power);
typedef void (*SetSyncWordCallback)(uint8_t syncWord);

struct RadioConfig {
  uint32_t freq_hz;
  uint32_t bw_hz;
  uint8_t sf;
  uint8_t cr;
  uint8_t tx_power;
  uint8_t sync_word;
};

class KissModem {
  Stream& _serial;
  mesh::LocalIdentity& _identity;
  mesh::RNG& _rng;
  
  uint8_t _rx_buf[KISS_MAX_FRAME_SIZE];
  uint16_t _rx_len;
  bool _rx_escaped;
  bool _rx_active;
  
  uint8_t _pending_tx[KISS_MAX_PACKET_SIZE];
  uint16_t _pending_tx_len;
  bool _has_pending_tx;

  SetRadioCallback _setRadioCallback;
  SetTxPowerCallback _setTxPowerCallback;
  SetSyncWordCallback _setSyncWordCallback;
  
  RadioConfig _config;

  void writeByte(uint8_t b);
  void writeFrame(uint8_t cmd, const uint8_t* data, uint16_t len);
  void writeErrorFrame(uint8_t error_code);
  void processFrame();
  
  void handleGetIdentity();
  void handleGetRandom(const uint8_t* data, uint16_t len);
  void handleVerifySignature(const uint8_t* data, uint16_t len);
  void handleSignData(const uint8_t* data, uint16_t len);
  void handleEncryptData(const uint8_t* data, uint16_t len);
  void handleDecryptData(const uint8_t* data, uint16_t len);
  void handleKeyExchange(const uint8_t* data, uint16_t len);
  void handleHash(const uint8_t* data, uint16_t len);
  void handleSetRadio(const uint8_t* data, uint16_t len);
  void handleSetTxPower(const uint8_t* data, uint16_t len);
  void handleSetSyncWord(const uint8_t* data, uint16_t len);
  void handleGetRadio();
  void handleGetTxPower();
  void handleGetSyncWord();
  void handleGetVersion();

public:
  KissModem(Stream& serial, mesh::LocalIdentity& identity, mesh::RNG& rng);
  
  void begin();
  void loop();
  
  void setRadioCallback(SetRadioCallback cb) { _setRadioCallback = cb; }
  void setTxPowerCallback(SetTxPowerCallback cb) { _setTxPowerCallback = cb; }
  void setSyncWordCallback(SetSyncWordCallback cb) { _setSyncWordCallback = cb; }
  
  bool getPacketToSend(uint8_t* packet, uint16_t* len);
  void onPacketReceived(int8_t snr, int8_t rssi, const uint8_t* packet, uint16_t len);
  void onTxComplete(bool success);
};
