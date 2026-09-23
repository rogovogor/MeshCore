#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "AbstractUITask.h"

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE 13

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "14 Aug 2026"
#endif

#ifndef FIRMWARE_VERSION
  #ifdef GIT_COMMIT
    #define FIRMWARE_VERSION "v1.17.1-" GIT_COMMIT
  #else
    #define FIRMWARE_VERSION "v1.17.1"
  #endif
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(ESP32)
#include <SPIFFS.h>
#endif

#include "DataStore.h"
#include "NodePrefs.h"

#ifdef WITH_WIFI_SWITCHING
  #include "WifiPrefs.h"
  #include <WiFi.h>
  #include <helpers/esp32/SerialBLEInterface.h>
  #include <helpers/esp32/SerialWifiInterface.h>
#endif

#include <RTClib.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/TimeSyncHelper.h>
#include <target.h>

/* ---------------------------------- CONFIGURATION ------------------------------------- */

#ifndef LORA_FREQ
#define LORA_FREQ 915.0
#endif
#ifndef LORA_BW
#define LORA_BW 250
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20
#endif
#ifndef MAX_LORA_TX_POWER
#define MAX_LORA_TX_POWER LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
#define MAX_CONTACTS 100
#endif

#ifndef OFFLINE_QUEUE_SIZE
#define OFFLINE_QUEUE_SIZE 16
#endif

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "MeshCore-"
#endif

#include <helpers/BaseChatMesh.h>
#include <helpers/TransportKeyStore.h>

#ifdef WITH_COMPANION_CLI
#include <helpers/CommonCLI.h>
#include "CompanionCLICallbacks.h"
#define TERMINAL_CLI_PSK  "VGVybWluYWxDTEkxMjM0NQ=="  // "TerminalCLI12345" — exactly 16 bytes
#endif

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS             0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE             0x02
#define REQ_TYPE_GET_TELEMETRY_DATA     0x03

struct AdvertPath {
  uint8_t pubkey_prefix[7];
  uint8_t path_len;
  char    name[32];
  uint32_t recv_timestamp;
  uint8_t path[MAX_PATH_SIZE];
};

class MyMesh : public BaseChatMesh, public DataStoreHost {
public:
  TimeSyncHelper _ts;

  enum TimeSource : uint8_t {
    TIME_SOURCE_UNSET = 0,
    TIME_SOURCE_CONTACTS,
    TIME_SOURCE_ADVERT,
    TIME_SOURCE_APP,
    TIME_SOURCE_RTC,
    TIME_SOURCE_GPS
  };

  MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui=NULL);

  void begin(bool has_display);
  void startInterface(BaseSerialInterface &serial);

  const char *getNodeName();
  NodePrefs *getNodePrefs();
  uint32_t getBLEPin();

  void loop();
  void handleCmdFrame(size_t len);
  bool advert();
  void enterCLIRescue();

  int  getRecentlyHeard(AdvertPath dest[], int max_num);
  TimeSource getTimeSource() const { return _time_source; }
  uint32_t getTimeSyncCount() const { return _ts._sync_count; }
  uint32_t getTimeLastSync() const { return _ts._last_sync; }
  int32_t getTimeLastAdjustment() const { return _ts._last_adj; }
  bool hasRecentAppTimeSet() const;
  const char *getTimeSourceLabel() const;

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  bool getCADEnabled() const override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet *packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet *packet) override;
  uint8_t getExtraAckTransmitCount() const override;
  bool filterRecvFloodPacket(mesh::Packet* packet) override;
  bool allowPacketForward(const mesh::Packet* packet) override;

  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis);
  void sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis=0) override;
  void sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis=0) override;

  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
  bool isAutoAddEnabled() const override;
  bool shouldAutoAddContactType(uint8_t type) const override;
  bool shouldOverwriteWhenFull() const override;
  uint8_t getAutoAddMaxHops() const override;
  void onContactsFull() override;
  void onContactOverwrite(const uint8_t* pub_key) override;
  bool onContactPathRecv(ContactInfo& from, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) override;
  void onContactPathUpdated(const ContactInfo &contact) override;
  ContactInfo* processAck(const uint8_t *data) override;
  void queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt, uint32_t sender_timestamp,
                    const uint8_t *extra, int extra_len, const char *text);

  void onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                     const char *text) override;
  void onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                         const char *text) override;
  void onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const uint8_t *sender_prefix, const char *text) override;
  void onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                            const char *text) override;
  void onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                         const uint8_t *data, size_t data_len) override;

  uint8_t onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                           uint8_t len, uint8_t *reply) override;
  void onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) override;
  void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                    const uint8_t* app_data, size_t app_data_len) override;
  void onControlDataRecv(mesh::Packet *packet) override;
  void onRawDataRecv(mesh::Packet *packet) override;
  void onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) override;

  uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
  uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;
  void onSendTimeout() override;

  // DataStoreHost methods
  bool onContactLoaded(const ContactInfo& contact) override { return addContact(contact); }
  bool getContactForSave(uint32_t idx, ContactInfo& contact) override { return getContactByIdx(idx, contact); }
  bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) override {
    // A stored record without a name is just a free slot. Loading it would call
    // setChannel(), which pulls num_channels up to channel_idx + 1: /channels2
    // holds up to MAX_GROUP_CHANNELS records, so num_channels ended up pinned at
    // MAX_GROUP_CHANNELS and addChannel("TerminalCLI") returned NULL forever.
    // Nodes that saved their channel list before the CLI existed stayed without
    // the channel. Empty records keep the slot empty without touching the count.
    if (!ch.name[0]) return true;
    return setChannel(channel_idx, ch);
  }
  bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) override { return getChannel(channel_idx, ch); }

  void clearPendingReqs() {
    pending_login = pending_status = pending_telemetry = pending_discovery = pending_req = 0;
  }

public:
  void savePrefs() {
    _prefs.node_lat = sensors.node_lat;
    _prefs.node_lon = sensors.node_lon;
    _store->savePrefs(_prefs);
  }
  void deferSavePrefs();   // schedule flash write to happen outside of BLE connection

#ifdef WITH_COMPANION_CLI
  const char* getCliPin() const { return _cli_pin; }

  int getChatMode() const {
    if ( _remote_cli_enabled &&  _terminal_cli_enabled) return 0;  // C+P
    if (!_remote_cli_enabled &&  _terminal_cli_enabled) return 1;  // cht
    if ( _remote_cli_enabled && !_terminal_cli_enabled) return 2;  // PM
    return 3;                                                       // OFF
  }
  void setChatMode(int m) {
    _remote_cli_enabled   = (m == 0 || m == 2);
    _terminal_cli_enabled = (m == 0 || m == 1);
  }

  int getTimesyncMode() const {
    if ( _ts_from_adverts &&  _ts_from_messages) return 0;  // a+g
    if (!_ts_from_adverts && !_ts_from_messages) return 1;  // gps
    return 2;                                                // adv
  }
  void setTimesyncMode(int m) {
    _ts_from_adverts  = (m == 0 || m == 2);
    _ts_from_messages = (m == 0);
  }

#endif

  bool isCyr2LatChannelsEnabled() const { return _cyr2lat_channels_enabled; }
  bool isCyr2LatContactsEnabled() const { return _cyr2lat_contacts_enabled; }
  void setCyr2LatChannelsEnabled(bool enabled);
  void setCyr2LatContactsEnabled(bool enabled);

#ifdef WITH_WIFI_SWITCHING
  void switchCommsMode(uint8_t mode, int wifi_net_idx = 0);
  bool isWifiConnecting() const { return _wifi_connecting; }
  bool isWifiConnected() const { return WiFi.status() == WL_CONNECTED; }
  String getWifiIP() const { return WiFi.localIP().toString(); }
  WifiPrefs* getWifiPrefs() { return &_wifi_prefs; }
  void addWifiNetwork(const char* ssid, const char* pass);
  void removeWifiNetwork(const char* ssid);
  void saveWifiPrefs();
  void loadWifiPrefs();
  void checkWifiConnection();
  void initCommsFromPrefs();
#endif

#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() {
    sensors.setSettingValue("gps", _prefs.gps_enabled ? "1" : "0");
    if (_prefs.gps_interval > 0) {
      char interval_str[12];  // Max: 24 hours = 86400 seconds (5 digits + null)
      sprintf(interval_str, "%u", _prefs.gps_interval);
      sensors.setSettingValue("gps_interval", interval_str);
    }
  }
#endif

  // To check if there is pending work
  bool hasPendingWork() const;

private:
  void writeOKFrame();
  void writeErrFrame(uint8_t err_code);
  void writeDisabledFrame();
  void writeContactRespFrame(uint8_t code, const ContactInfo &contact);
  void updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len);
  void addToOfflineQueue(const uint8_t frame[], int len);
  int getFromOfflineQueue(uint8_t frame[]);
  int getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) override { 
    return _store->getBlobByKey(key, key_len, dest_buf);
  }
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], int len) override {
    return _store->putBlobByKey(key, key_len, src_buf, len);
  }

  void checkCLIRescueCmd();
  void checkSerialInterface();
  // True while the connected app still has frames to receive: queued in the
  // transport, or waiting in the offline queue for its next SYNC_NEXT_MESSAGE.
  bool hasUndeliveredAppFrames() const;
  // Postpone a due reboot/power-off while those frames drain, re-arming
  // action_at. Bounded by APP_DRAIN_GRACE_MILLIS so an app that never syncs
  // cannot defer the action indefinitely.
  bool deferForAppDrain(unsigned long& action_at);
  bool isValidClientRepeatFreq(uint32_t f) const;

  // helpers, short-cuts
  void saveChannels() { _store->saveChannels(this); }
  void saveContacts();

  DataStore* _store;
  NodePrefs _prefs;
  uint32_t pending_login;
  uint32_t pending_status;
  uint32_t pending_telemetry, pending_discovery;   // pending _TELEMETRY_REQ
  uint32_t pending_req;   // pending _BINARY_REQ
  BaseSerialInterface *_serial;
  AbstractUITask* _ui;

  ContactsIterator _iter;
  uint32_t _iter_filter_since;
  uint32_t _most_recent_lastmod;
  uint32_t _active_ble_pin;
  bool _iter_started;
  bool _cli_rescue;
  bool send_unscoped;   // force un-scoped flood (instead of using send_scope)
  char cli_command[80];
  uint8_t app_target_ver;
  uint8_t *sign_data;
  uint32_t sign_data_len;
  unsigned long dirty_contacts_expiry;
  unsigned long dirty_prefs_expiry;

  TransportKey send_scope;

  uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
  uint8_t out_frame[MAX_FRAME_SIZE + 1];
  CayenneLPP telemetry;
  TimeSource _time_source = TIME_SOURCE_UNSET;
  uint32_t _app_time_lock_until = 0;
  void noteTimeSource(TimeSource source);

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];

    bool isChannelMsg() const;
  };
  int offline_queue_len;
  Frame offline_queue[OFFLINE_QUEUE_SIZE];

  struct AckTableEntry {
    unsigned long msg_sent;
    uint32_t ack;
    ContactInfo* contact;
  };
  #define EXPECTED_ACK_TABLE_SIZE 8
  AckTableEntry expected_ack_table[EXPECTED_ACK_TABLE_SIZE]; // circular table
  int next_ack_idx;

  #define ADVERT_PATH_TABLE_SIZE   16
  AdvertPath advert_paths[ADVERT_PATH_TABLE_SIZE]; // circular table

  struct Cyr2LatChannelMap {
    uint8_t transformed_hash[MAX_HASH_SIZE];
    uint16_t original_payload_len;
    uint8_t original_payload[MAX_PACKET_PAYLOAD];
  };
  #define CYR2LAT_CHANNEL_MAP_SIZE 4
  Cyr2LatChannelMap      _cyr2lat_channel_maps[CYR2LAT_CHANNEL_MAP_SIZE] = {};
  int                    _next_cyr2lat_channel_map = 0;
  bool                   _cyr2lat_channels_enabled = false;
  bool                   _cyr2lat_contacts_enabled = false;
  unsigned long          _pending_reboot_at = 0;
  unsigned long          _pending_reboot_deadline = 0;
  unsigned long          _pending_poweroff_at = 0;
  unsigned long          _pending_poweroff_deadline = 0;
  unsigned long          _app_drain_until = 0;      // 0 = no drain window open

  bool sendGroupMessageWithCyr2LatMap(uint32_t timestamp, mesh::GroupChannel& channel, const char* sender_name,
                                      const char* text, int text_len, const char* original_text,
                                      int original_len, bool record_map);
  int mapCyr2LatChannelRawLog(const uint8_t* raw, int len, uint8_t* mapped, int mapped_size);

#ifdef WITH_COMPANION_CLI
  CommonCLI*             _cli = nullptr;
  CompanionCLICallbacks* _cli_cb = nullptr;
  char                   _cli_pin[9];
  bool                   _remote_cli_enabled = true;
  bool                   _terminal_cli_enabled = true;
  bool                   _ts_from_adverts = true;
  bool                   _ts_from_messages = true;

  void handleRemoteCLI(const ContactInfo& from, uint32_t sender_ts, const char* cmd);
  void handleTerminalCLI(uint8_t ch_idx, uint32_t sender_ts, const char* cmd,
                         bool mirror_ui_reply);
  void sendCliReplyPM(const ContactInfo& to, const char* buf);
  void sendCliReplyChannel(uint8_t ch_idx, const char* buf, bool mirror_ui = false);
  void injectChannelMsg(uint8_t ch_idx, const char* sender_name, uint8_t path_len, int8_t snr_x4, uint32_t ts, const char* text);
  int  findTerminalCLIChannelIdx();
  bool handleCliCmd(uint32_t sender_ts, const char* cmd, char* buf, bool is_remote);
#endif

#ifdef WITH_WIFI_SWITCHING
  SerialBLEInterface  _ble_iface;
  SerialWifiInterface _wifi_iface;
  WifiPrefs           _wifi_prefs;
  bool                _wifi_connecting = false;
  uint32_t            _wifi_connect_start = 0;
  int                 _wifi_net_idx = -1;
  uint32_t            _wifi_bg_retry_at = 0;
#endif
};

extern MyMesh the_mesh;
