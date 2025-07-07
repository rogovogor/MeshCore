#pragma once

#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(ESP32)
#include <SPIFFS.h>
#endif

#include <helpers/ArduinoHelpers.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/IdentityStore.h>
#include <helpers/AdvertDataHelpers.h>
#include <helpers/TxtDataHelpers.h>
#include <helpers/CommonCLI.h>
#include <RTClib.h>
#include <target.h>

struct ContactInfo {
  mesh::Identity id;
  uint8_t type;   // 1 = admin, 0 = guest
  uint8_t flags;
  int8_t out_path_len;
  uint8_t out_path[MAX_PATH_SIZE];
  uint8_t shared_secret[PUB_KEY_SIZE];
  uint32_t last_timestamp;   // by THEIR clock  (transient)
  uint32_t last_activity;    // by OUR clock    (transient)

  bool isAdmin() const { return type != 0; }
};

#ifndef FIRMWARE_BUILD_DATE
  #define FIRMWARE_BUILD_DATE   "2 Jul 2025"
#endif

#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION   "v1.7.2"
#endif

#define FIRMWARE_ROLE "sensor"

#ifndef MAX_CONTACTS
  #define MAX_CONTACTS           32
#endif

#define MAX_SEARCH_RESULTS   8

class SensorMesh : public mesh::Mesh, public CommonCLICallbacks {
public:
  SensorMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng, mesh::RTCClock& rtc, mesh::MeshTables& tables);
  void begin(FILESYSTEM* fs);
  CommonCLI* getCLI() { return &_cli; }
  void loop();

  // CommonCLI callbacks
  const char* getFirmwareVer() override { return FIRMWARE_VERSION; }
  const char* getBuildDate() override { return FIRMWARE_BUILD_DATE; }
  const char* getRole() override { return FIRMWARE_ROLE; }
  const char* getNodeName() { return _prefs.node_name; }
  NodePrefs* getNodePrefs() { return &_prefs; }
  void savePrefs() override { _cli.savePrefs(_fs); }
  bool formatFileSystem() override;
  void sendSelfAdvertisement(int delay_millis) override;
  void updateAdvertTimer() override;
  void updateFloodAdvertTimer() override;
  void setLoggingOn(bool enable) override {  }
  void eraseLogFile() override { }
  void dumpLogFile() override { }
  void setTxPower(uint8_t power_dbm) override;
  void formatNeighborsReply(char *reply) override {
    strcpy(reply, "not supported");
  }
  const uint8_t* getSelfIdPubKey() override { return self_id.pub_key; }
  void clearStats() override { }

protected:
  // telemetry data queries
  float getVoltage(uint8_t channel) { return 0.0f; }  // TODO: extract from curr telemetry buffer

  // alerts
  struct Trigger {
    bool triggered;
    uint32_t time;

    Trigger() { triggered = false; time = 0; }
  };

  void alertIfLow(Trigger& t, float value, float threshold, const char* text);
  void alertIfHigh(Trigger& t, float value, float threshold, const char* text);

  virtual void checkForAlerts() = 0;   // for app to implement

  // Mesh overrides
  float getAirtimeBudgetFactor() const override;
  bool allowPacketForward(const mesh::Packet* packet) override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet* packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet* packet) override;
  int getInterferenceThreshold() const override;
  int getAGCResetInterval() const override;
  void onAnonDataRecv(mesh::Packet* packet, uint8_t type, const mesh::Identity& sender, uint8_t* data, size_t len) override;
  int searchPeersByHash(const uint8_t* hash) override;
  void getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) override;
  void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp, const uint8_t* app_data, size_t app_data_len);
  void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx, const uint8_t* secret, uint8_t* data, size_t len) override;
  bool onPeerPathRecv(mesh::Packet* packet, int sender_idx, const uint8_t* secret, uint8_t* path, uint8_t path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;

private:
  FILESYSTEM* _fs;
  unsigned long next_local_advert, next_flood_advert;
  NodePrefs _prefs;
  CommonCLI _cli;
  uint8_t reply_data[MAX_PACKET_PAYLOAD];
  ContactInfo contacts[MAX_CONTACTS];
  int num_contacts;
  unsigned long dirty_contacts_expiry;
  CayenneLPP telemetry;
  uint32_t last_read_time;
  int matching_peer_indexes[MAX_SEARCH_RESULTS];

  void loadContacts();
  void saveContacts();
  int handleRequest(ContactInfo& sender, uint32_t sender_timestamp, uint8_t* payload, size_t payload_len);
  mesh::Packet* createSelfAdvert();
  ContactInfo* putContact(const mesh::Identity& id);

  void sendAlert(const char* text) { }  // TODO

};
