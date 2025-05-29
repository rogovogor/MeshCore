#ifndef MYMESH_H
#define MYMESH_H

#include <Arduino.h>
#include <Mesh.h>
#ifdef DISPLAY_CLASS
  #include "UITask.h"
#endif

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE    5

#ifndef FIRMWARE_BUILD_DATE
  #define FIRMWARE_BUILD_DATE   "24 May 2025"
#endif

#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION   "v1.6.2"
#endif

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
#include <helpers/BaseSerialInterface.h>
#include "NodePrefs.h"
#include <RTClib.h>
#include <target.h>

/* ---------------------------------- CONFIGURATION ------------------------------------- */

#ifndef LORA_FREQ
  #define LORA_FREQ   915.0
#endif
#ifndef LORA_BW
  #define LORA_BW     250
#endif
#ifndef LORA_SF
  #define LORA_SF     10
#endif
#ifndef LORA_CR
  #define LORA_CR      5
#endif
#ifndef LORA_TX_POWER
  #define LORA_TX_POWER  20
#endif
#ifndef MAX_LORA_TX_POWER
  #define MAX_LORA_TX_POWER  LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
  #define MAX_CONTACTS         100
#endif

#ifndef OFFLINE_QUEUE_SIZE
  #define OFFLINE_QUEUE_SIZE  16
#endif

#ifndef BLE_NAME_PREFIX
  #define BLE_NAME_PREFIX  "MeshCore-"
#endif

#include <helpers/BaseChatMesh.h>

#define SEND_TIMEOUT_BASE_MILLIS          500
#define FLOOD_SEND_TIMEOUT_FACTOR         16.0f
#define DIRECT_SEND_PERHOP_FACTOR         6.0f
#define DIRECT_SEND_PERHOP_EXTRA_MILLIS   250
#define LAZY_CONTACTS_WRITE_DELAY        5000

#define  PUBLIC_GROUP_PSK  "izOH6cXN6mrJ5e26oRXNcg=="

#define CMD_APP_START              1
#define CMD_SEND_TXT_MSG           2
#define CMD_SEND_CHANNEL_TXT_MSG   3
#define CMD_GET_CONTACTS           4   // with optional 'since' (for efficient sync)
#define CMD_GET_DEVICE_TIME        5
#define CMD_SET_DEVICE_TIME        6
#define CMD_SEND_SELF_ADVERT       7
#define CMD_SET_ADVERT_NAME        8
#define CMD_ADD_UPDATE_CONTACT     9
#define CMD_SYNC_NEXT_MESSAGE     10
#define CMD_SET_RADIO_PARAMS      11
#define CMD_SET_RADIO_TX_POWER    12
#define CMD_RESET_PATH            13
#define CMD_SET_ADVERT_LATLON     14
#define CMD_REMOVE_CONTACT        15
#define CMD_SHARE_CONTACT         16
#define CMD_EXPORT_CONTACT        17
#define CMD_IMPORT_CONTACT        18
#define CMD_REBOOT                19
#define CMD_GET_BATTERY_VOLTAGE   20
#define CMD_SET_TUNING_PARAMS     21
#define CMD_DEVICE_QEURY          22
#define CMD_EXPORT_PRIVATE_KEY    23
#define CMD_IMPORT_PRIVATE_KEY    24
#define CMD_SEND_RAW_DATA         25
#define CMD_SEND_LOGIN            26
#define CMD_SEND_STATUS_REQ       27
#define CMD_HAS_CONNECTION        28
#define CMD_LOGOUT                29   // 'Disconnect'
#define CMD_GET_CONTACT_BY_KEY    30
#define CMD_GET_CHANNEL           31
#define CMD_SET_CHANNEL           32
#define CMD_SIGN_START            33
#define CMD_SIGN_DATA             34
#define CMD_SIGN_FINISH           35
#define CMD_SEND_TRACE_PATH       36
#define CMD_SET_DEVICE_PIN        37
#define CMD_SET_OTHER_PARAMS      38
#define CMD_SEND_TELEMETRY_REQ    39
#define CMD_GET_CUSTOM_VARS       40
#define CMD_SET_CUSTOM_VAR        41

#define RESP_CODE_OK                0
#define RESP_CODE_ERR               1
#define RESP_CODE_CONTACTS_START    2   // first reply to CMD_GET_CONTACTS
#define RESP_CODE_CONTACT           3   // multiple of these (after CMD_GET_CONTACTS)
#define RESP_CODE_END_OF_CONTACTS   4   // last reply to CMD_GET_CONTACTS
#define RESP_CODE_SELF_INFO         5   // reply to CMD_APP_START
#define RESP_CODE_SENT              6   // reply to CMD_SEND_TXT_MSG
#define RESP_CODE_CONTACT_MSG_RECV  7   // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CHANNEL_MSG_RECV  8   // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CURR_TIME         9   // a reply to CMD_GET_DEVICE_TIME
#define RESP_CODE_NO_MORE_MESSAGES 10   // a reply to CMD_SYNC_NEXT_MESSAGE
#define RESP_CODE_EXPORT_CONTACT   11
#define RESP_CODE_BATTERY_VOLTAGE  12   // a reply to a CMD_GET_BATTERY_VOLTAGE
#define RESP_CODE_DEVICE_INFO      13   // a reply to CMD_DEVICE_QEURY
#define RESP_CODE_PRIVATE_KEY      14   // a reply to CMD_EXPORT_PRIVATE_KEY
#define RESP_CODE_DISABLED         15
#define RESP_CODE_CONTACT_MSG_RECV_V3  16   // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_MSG_RECV_V3  17   // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_INFO     18   // a reply to CMD_GET_CHANNEL
#define RESP_CODE_SIGN_START       19
#define RESP_CODE_SIGNATURE        20
#define RESP_CODE_CUSTOM_VARS      21

// these are _pushed_ to client app at any time
#define PUSH_CODE_ADVERT            0x80
#define PUSH_CODE_PATH_UPDATED      0x81
#define PUSH_CODE_SEND_CONFIRMED    0x82
#define PUSH_CODE_MSG_WAITING       0x83
#define PUSH_CODE_RAW_DATA          0x84
#define PUSH_CODE_LOGIN_SUCCESS     0x85
#define PUSH_CODE_LOGIN_FAIL        0x86
#define PUSH_CODE_STATUS_RESPONSE   0x87
#define PUSH_CODE_LOG_RX_DATA       0x88
#define PUSH_CODE_TRACE_DATA        0x89
#define PUSH_CODE_NEW_ADVERT        0x8A
#define PUSH_CODE_TELEMETRY_RESPONSE  0x8B

#define ERR_CODE_UNSUPPORTED_CMD      1
#define ERR_CODE_NOT_FOUND            2
#define ERR_CODE_TABLE_FULL           3
#define ERR_CODE_BAD_STATE            4
#define ERR_CODE_FILE_IO_ERROR        5
#define ERR_CODE_ILLEGAL_ARG          6

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS          0x01   // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE          0x02
#define REQ_TYPE_GET_TELEMETRY_DATA  0x03

#define MAX_SIGN_DATA_LEN    (8*1024)   // 8K

class MyMesh : public BaseChatMesh {
public:
    MyMesh(mesh::Radio& radio, mesh::RNG& rng, mesh::RTCClock& rtc, SimpleMeshTables& tables);
    
    void begin(FILESYSTEM& fs, bool has_display);
    void startInterface(BaseSerialInterface& serial);
    void loadPrefsInt(const char* filename);
    void savePrefs();
    
    const char* getNodeName();
    NodePrefs* getNodePrefs();
    uint32_t getBLEPin();

    void loop();
    void handleCmdFrame(size_t len);
    bool advert();

protected:
    float getAirtimeBudgetFactor() const override;
    int getInterferenceThreshold() const override;
    int calcRxDelay(float score, uint32_t air_time) const override;

    void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
    bool isAutoAddEnabled() const override;
    void onDiscoveredContact(ContactInfo& contact, bool is_new) override;
    void onContactPathUpdated(const ContactInfo& contact) override;
    bool processAck(const uint8_t *data) override;
    void queueMessage(const ContactInfo& from, uint8_t txt_type, mesh::Packet* pkt,
                      uint32_t sender_timestamp, const uint8_t* extra, int extra_len, const char *text);

    void onMessageRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const char *text) override;
    void onCommandDataRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const char *text) override;
    void onSignedMessageRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp,
                             const uint8_t *sender_prefix, const char *text) override;
    void onChannelMessageRecv(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t timestamp, const char *text) override;

    uint8_t onContactRequest(const ContactInfo& contact, uint32_t sender_timestamp, const uint8_t* data, uint8_t len, uint8_t* reply) override;
    void onContactResponse(const ContactInfo& contact, const uint8_t* data, uint8_t len) override;
    void onRawDataRecv(mesh::Packet* packet) override;
    void onTraceRecv(mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags, const uint8_t* path_snrs,
                     const uint8_t* path_hashes, uint8_t path_len) override;

    uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
    uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;
    void onSendTimeout() override;

private:
    void writeOKFrame();
    void writeErrFrame(uint8_t err_code);
    void writeDisabledFrame();
    void writeContactRespFrame(uint8_t code, const ContactInfo& contact);
    void updateContactFromFrame(ContactInfo& contact, const uint8_t* frame, int len);
    void addToOfflineQueue(const uint8_t frame[], int len);
    int getFromOfflineQueue(uint8_t frame[]);
    void loadMainIdentity();
    bool saveMainIdentity(const mesh::LocalIdentity& identity);
    void loadContacts();
    void saveContacts();
    void loadChannels();
    void saveChannels();
    int getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) override;
    bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], int len) override;

private:
    FILESYSTEM* _fs;
    IdentityStore* _identity_store;
    NodePrefs _prefs;
    uint32_t pending_login;
    uint32_t pending_status;
    uint32_t pending_telemetry;
    BaseSerialInterface* _serial;

    ContactsIterator _iter;
    uint32_t _iter_filter_since;
    uint32_t _most_recent_lastmod;
    uint32_t _active_ble_pin;
    bool _iter_started;
    uint8_t app_target_ver;
    uint8_t* sign_data;
    uint32_t sign_data_len;
    unsigned long dirty_contacts_expiry;

    uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
    uint8_t out_frame[MAX_FRAME_SIZE + 1];
    CayenneLPP telemetry;

    struct Frame {
        uint8_t len;
        uint8_t buf[MAX_FRAME_SIZE];
    };
    int offline_queue_len;
    Frame offline_queue[OFFLINE_QUEUE_SIZE];

    struct AckTableEntry {
        unsigned long msg_sent;
        uint32_t ack;
    };
    #define EXPECTED_ACK_TABLE_SIZE   8
    AckTableEntry  expected_ack_table[EXPECTED_ACK_TABLE_SIZE];  // circular table
    int next_ack_idx;
};

extern StdRNG fast_rng;
extern SimpleMeshTables tables;
extern MyMesh the_mesh;
#ifdef DISPLAY_CLASS
  extern UITask ui_task;
#endif
#endif // MYMESH_H