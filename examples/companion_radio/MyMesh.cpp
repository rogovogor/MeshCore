#include "MyMesh.h"

#include <Arduino.h> // needed for PlatformIO
#include <Mesh.h>

#define CMD_APP_START                 1
#define CMD_SEND_TXT_MSG              2
#define CMD_SEND_CHANNEL_TXT_MSG      3
#define CMD_GET_CONTACTS              4 // with optional 'since' (for efficient sync)
#define CMD_GET_DEVICE_TIME           5
#define CMD_SET_DEVICE_TIME           6
#define CMD_SEND_SELF_ADVERT          7
#define CMD_SET_ADVERT_NAME           8
#define CMD_ADD_UPDATE_CONTACT        9
#define CMD_SYNC_NEXT_MESSAGE         10
#define CMD_SET_RADIO_PARAMS          11
#define CMD_SET_RADIO_TX_POWER        12
#define CMD_RESET_PATH                13
#define CMD_SET_ADVERT_LATLON         14
#define CMD_REMOVE_CONTACT            15
#define CMD_SHARE_CONTACT             16
#define CMD_EXPORT_CONTACT            17
#define CMD_IMPORT_CONTACT            18
#define CMD_REBOOT                    19
#define CMD_GET_BATT_AND_STORAGE      20   // was CMD_GET_BATTERY_VOLTAGE
#define CMD_SET_TUNING_PARAMS         21
#define CMD_DEVICE_QUERY              22
#define CMD_EXPORT_PRIVATE_KEY        23
#define CMD_IMPORT_PRIVATE_KEY        24
#define CMD_SEND_RAW_DATA             25
#define CMD_SEND_LOGIN                26
#define CMD_SEND_STATUS_REQ           27
#define CMD_HAS_CONNECTION            28
#define CMD_LOGOUT                    29 // 'Disconnect'
#define CMD_GET_CONTACT_BY_KEY        30
#define CMD_GET_CHANNEL               31
#define CMD_SET_CHANNEL               32
#define CMD_SIGN_START                33
#define CMD_SIGN_DATA                 34
#define CMD_SIGN_FINISH               35
#define CMD_SEND_TRACE_PATH           36
#define CMD_SET_DEVICE_PIN            37
#define CMD_SET_OTHER_PARAMS          38
#define CMD_SEND_TELEMETRY_REQ        39  // can deprecate this
#define CMD_GET_CUSTOM_VARS           40
#define CMD_SET_CUSTOM_VAR            41
#define CMD_GET_ADVERT_PATH           42
#define CMD_GET_TUNING_PARAMS         43
// NOTE: CMD range 44..49 parked, potentially for WiFi operations
#define CMD_SEND_BINARY_REQ           50
#define CMD_FACTORY_RESET             51
#define CMD_SEND_PATH_DISCOVERY_REQ   52
#define CMD_SET_FLOOD_SCOPE_KEY       54   // v8+
#define CMD_SEND_CONTROL_DATA         55   // v8+
#define CMD_GET_STATS                 56   // v8+, second byte is stats type
#define CMD_SEND_ANON_REQ             57
#define CMD_SET_AUTOADD_CONFIG        58
#define CMD_GET_AUTOADD_CONFIG        59
#define CMD_GET_ALLOWED_REPEAT_FREQ   60
#define CMD_SET_PATH_HASH_MODE        61
#define CMD_SEND_CHANNEL_DATA         62
#define CMD_SET_DEFAULT_FLOOD_SCOPE   63
#define CMD_GET_DEFAULT_FLOOD_SCOPE   64
#define CMD_SEND_RAW_PACKET           65

// Stats sub-types for CMD_GET_STATS
#define STATS_TYPE_CORE               0
#define STATS_TYPE_RADIO              1
#define STATS_TYPE_PACKETS             2

#define RESP_CODE_OK                  0
#define RESP_CODE_ERR                 1
#define RESP_CODE_CONTACTS_START      2  // first reply to CMD_GET_CONTACTS
#define RESP_CODE_CONTACT             3  // multiple of these (after CMD_GET_CONTACTS)
#define RESP_CODE_END_OF_CONTACTS     4  // last reply to CMD_GET_CONTACTS
#define RESP_CODE_SELF_INFO           5  // reply to CMD_APP_START
#define RESP_CODE_SENT                6  // reply to CMD_SEND_TXT_MSG
#define RESP_CODE_CONTACT_MSG_RECV    7  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CHANNEL_MSG_RECV    8  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CURR_TIME           9  // a reply to CMD_GET_DEVICE_TIME
#define RESP_CODE_NO_MORE_MESSAGES    10 // a reply to CMD_SYNC_NEXT_MESSAGE
#define RESP_CODE_EXPORT_CONTACT      11
#define RESP_CODE_BATT_AND_STORAGE    12 // a reply to a CMD_GET_BATT_AND_STORAGE
#define RESP_CODE_DEVICE_INFO         13 // a reply to CMD_DEVICE_QUERY
#define RESP_CODE_PRIVATE_KEY         14 // a reply to CMD_EXPORT_PRIVATE_KEY
#define RESP_CODE_DISABLED            15
#define RESP_CODE_CONTACT_MSG_RECV_V3 16 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_MSG_RECV_V3 17 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_INFO        18 // a reply to CMD_GET_CHANNEL
#define RESP_CODE_SIGN_START          19
#define RESP_CODE_SIGNATURE           20
#define RESP_CODE_CUSTOM_VARS         21
#define RESP_CODE_ADVERT_PATH         22
#define RESP_CODE_TUNING_PARAMS       23
#define RESP_CODE_STATS               24   // v8+, second byte is stats type
#define RESP_CODE_AUTOADD_CONFIG      25
#define RESP_ALLOWED_REPEAT_FREQ      26
#define RESP_CODE_CHANNEL_DATA_RECV   27
#define RESP_CODE_DEFAULT_FLOOD_SCOPE 28

#define MAX_CHANNEL_DATA_LENGTH       (MAX_FRAME_SIZE - 9)

#define SEND_TIMEOUT_BASE_MILLIS        500
#define FLOOD_SEND_TIMEOUT_FACTOR       16.0f
#define DIRECT_SEND_PERHOP_FACTOR       6.0f
#define DIRECT_SEND_PERHOP_EXTRA_MILLIS 250
#define LAZY_CONTACTS_WRITE_DELAY       5000
#define LAZY_PREFS_WRITE_DELAY          2000

#define PUBLIC_GROUP_PSK                "izOH6cXN6mrJ5e26oRXNcg=="

// these are _pushed_ to client app at any time
#define PUSH_CODE_ADVERT                0x80
#define PUSH_CODE_PATH_UPDATED          0x81
#define PUSH_CODE_SEND_CONFIRMED        0x82
#define PUSH_CODE_MSG_WAITING           0x83
#define PUSH_CODE_RAW_DATA              0x84
#define PUSH_CODE_LOGIN_SUCCESS         0x85
#define PUSH_CODE_LOGIN_FAIL            0x86
#define PUSH_CODE_STATUS_RESPONSE       0x87
#define PUSH_CODE_LOG_RX_DATA           0x88
#define PUSH_CODE_TRACE_DATA            0x89
#define PUSH_CODE_NEW_ADVERT            0x8A
#define PUSH_CODE_TELEMETRY_RESPONSE    0x8B
#define PUSH_CODE_BINARY_RESPONSE       0x8C
#define PUSH_CODE_PATH_DISCOVERY_RESPONSE 0x8D
#define PUSH_CODE_CONTROL_DATA          0x8E   // v8+
#define PUSH_CODE_CONTACT_DELETED       0x8F // used to notify client app of deleted contact when overwriting oldest
#define PUSH_CODE_CONTACTS_FULL         0x90 // used to notify client app that contacts storage is full

#define ERR_CODE_UNSUPPORTED_CMD        1
#define ERR_CODE_NOT_FOUND              2
#define ERR_CODE_TABLE_FULL             3
#define ERR_CODE_BAD_STATE              4
#define ERR_CODE_FILE_IO_ERROR          5
#define ERR_CODE_ILLEGAL_ARG            6

#define MAX_SIGN_DATA_LEN               (8 * 1024) // 8K

// Auto-add config bitmask
// Bit 0: If set, overwrite oldest non-favourite contact when contacts file is full
// Bits 1-4: these indicate which contact types to auto-add when manual_contact_mode = 0x01

namespace {

bool mapCyrToLatUtf8(uint8_t b0, uint8_t b1, char& out) {
  if (b0 == 0xD0) {
    switch (b1) {
      case 0x81: out = 'E'; return true; // Ё
      case 0x90: out = 'A'; return true; // А
      case 0x92: out = 'B'; return true; // В
      case 0x95: out = 'E'; return true; // Е
      case 0x97: out = '3'; return true; // З
      case 0x9A: out = 'K'; return true; // К
      case 0x9C: out = 'M'; return true; // М
      case 0x9D: out = 'H'; return true; // Н
      case 0x9E: out = 'O'; return true; // О
      case 0xA0: out = 'P'; return true; // Р
      case 0xA1: out = 'C'; return true; // С
      case 0xA2: out = 'T'; return true; // Т
      case 0xA5: out = 'X'; return true; // Х
      case 0xAC: out = 'b'; return true; // Ь
      case 0xB0: out = 'a'; return true; // а
      case 0xB5: out = 'e'; return true; // е
      case 0xBE: out = 'o'; return true; // о
      default: break;
    }
  } else if (b0 == 0xD1) {
    switch (b1) {
      case 0x80: out = 'p'; return true; // р
      case 0x81: out = 'c'; return true; // с
      case 0x83: out = 'y'; return true; // у
      case 0x85: out = 'x'; return true; // х
      case 0x91: out = 'e'; return true; // ё
      default: break;
    }
  }
  return false;
}

int transliterateChannelText(const char* src, int src_len, char* dest, int dest_size) {
  if (dest_size <= 0) return 0;

  int di = 0;
  for (int si = 0; si < src_len && di < dest_size - 1; ) {
    uint8_t b0 = (uint8_t)src[si];
    char mapped;
    if (si + 1 < src_len && mapCyrToLatUtf8(b0, (uint8_t)src[si + 1], mapped)) {
      dest[di++] = mapped;
      si += 2;
    } else {
      dest[di++] = src[si++];
    }
  }

  dest[di] = '\0';
  return di;
}

bool textDiffers(const char* a, int a_len, const char* b, int b_len) {
  return a_len != b_len || memcmp(a, b, a_len) != 0;
}

}
#define AUTO_ADD_OVERWRITE_OLDEST (1 << 0)  // 0x01 - overwrite oldest non-favourite when full
#define AUTO_ADD_CHAT             (1 << 1)  // 0x02 - auto-add Chat (Companion) (ADV_TYPE_CHAT)
#define AUTO_ADD_REPEATER         (1 << 2)  // 0x04 - auto-add Repeater (ADV_TYPE_REPEATER)
#define AUTO_ADD_ROOM_SERVER      (1 << 3)  // 0x08 - auto-add Room Server (ADV_TYPE_ROOM)
#define AUTO_ADD_SENSOR           (1 << 4)  // 0x10 - auto-add Sensor (ADV_TYPE_SENSOR)

static const uint32_t MIN_VALID_TS = 1577836800; // 2020-01-01 UTC
static const uint32_t MAX_VALID_TS = 2524608000; // 2050-01-01 UTC
static const uint32_t APP_TIME_HOLDOFF_SECS = 6 * 60 * 60;

void MyMesh::writeOKFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_OK;
  _serial->writeFrame(buf, 1);
}
void MyMesh::writeErrFrame(uint8_t err_code) {
  uint8_t buf[2];
  buf[0] = RESP_CODE_ERR;
  buf[1] = err_code;
  _serial->writeFrame(buf, 2);
}

void MyMesh::writeDisabledFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_DISABLED;
  _serial->writeFrame(buf, 1);
}

void MyMesh::writeContactRespFrame(uint8_t code, const ContactInfo &contact) {
  int i = 0;
  out_frame[i++] = code;
  memcpy(&out_frame[i], contact.id.pub_key, PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  out_frame[i++] = contact.type;
  out_frame[i++] = contact.flags;
  out_frame[i++] = contact.out_path_len;
  memcpy(&out_frame[i], contact.out_path, MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  StrHelper::strzcpy((char *)&out_frame[i], contact.name, 32);
  i += 32;
  memcpy(&out_frame[i], &contact.last_advert_timestamp, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lat, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lon, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.lastmod, 4);
  i += 4;
  _serial->writeFrame(out_frame, i);
}

void MyMesh::updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len) {
  int i = 0;
  uint8_t code = frame[i++]; // eg. CMD_ADD_UPDATE_CONTACT
  memcpy(contact.id.pub_key, &frame[i], PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  contact.type = frame[i++];
  contact.flags = frame[i++];
  contact.out_path_len = frame[i++];
  memcpy(contact.out_path, &frame[i], MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  memcpy(contact.name, &frame[i], 32);
  i += 32;
  memcpy(&contact.last_advert_timestamp, &frame[i], 4);
  i += 4;
  if (len >= i + 8) { // optional fields
    memcpy(&contact.gps_lat, &frame[i], 4);
    i += 4;
    memcpy(&contact.gps_lon, &frame[i], 4);
    i += 4;
    if (len >= i + 4) {
      memcpy(&last_mod, &frame[i], 4);
    }
  }
}

bool MyMesh::Frame::isChannelMsg() const {
  return buf[0] == RESP_CODE_CHANNEL_MSG_RECV || buf[0] == RESP_CODE_CHANNEL_MSG_RECV_V3 ||
         buf[0] == RESP_CODE_CHANNEL_DATA_RECV;
}

void MyMesh::addToOfflineQueue(const uint8_t frame[], int len) {
  if (offline_queue_len >= OFFLINE_QUEUE_SIZE) {
    MESH_DEBUG_PRINTLN("WARN: offline_queue is full!");
    int pos = 0;
    while (pos < offline_queue_len) {
      if (offline_queue[pos].isChannelMsg()) {
        for (int i = pos; i < offline_queue_len - 1; i++) { // delete oldest channel msg from queue
          offline_queue[i] = offline_queue[i + 1];
        }
        MESH_DEBUG_PRINTLN("INFO: removed oldest channel message from queue.");
        offline_queue[offline_queue_len - 1].len = len;
        memcpy(offline_queue[offline_queue_len - 1].buf, frame, len);
        return;
      }
      pos++;
    }
    MESH_DEBUG_PRINTLN("INFO: no channel messages to remove from queue.");
  } else {
    offline_queue[offline_queue_len].len = len;
    memcpy(offline_queue[offline_queue_len].buf, frame, len);
    offline_queue_len++;
  }
}

int MyMesh::getFromOfflineQueue(uint8_t frame[]) {
  if (offline_queue_len > 0) {         // check offline queue
    size_t len = offline_queue[0].len; // take from top of queue
    memcpy(frame, offline_queue[0].buf, len);

    offline_queue_len--;
    for (int i = 0; i < offline_queue_len; i++) { // delete top item from queue
      offline_queue[i] = offline_queue[i + 1];
    }
    return len;
  }
  return 0; // queue is empty
}

float MyMesh::getAirtimeBudgetFactor() const {
  return _prefs.airtime_factor;
}

int MyMesh::getInterferenceThreshold() const {
  return 0; // disabled for now, until currentRSSI() problem is resolved
}
bool MyMesh::getCADEnabled() const {
  return false; // hardware CAD before TX (disabled by default, until configurable)
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int)((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * 0.5f);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * 0.2f);
  return getRNG()->nextInt(0, 5*t + 1);
}

uint8_t MyMesh::getExtraAckTransmitCount() const {
  return _prefs.multi_acks;
}

void MyMesh::logRxRaw(float snr, float rssi, const uint8_t raw[], int len) {
  const uint8_t* log_raw = raw;
  int log_len = len;
  uint8_t mapped[MAX_TRANS_UNIT];
  int mapped_len = mapCyr2LatChannelRawLog(raw, len, mapped, sizeof(mapped));
  if (mapped_len > 0) {
    log_raw = mapped;
    log_len = mapped_len;
  }
  if (_serial->isConnected() && log_len + 3 <= MAX_FRAME_SIZE) {
    int i = 0;
    out_frame[i++] = PUSH_CODE_LOG_RX_DATA;
    out_frame[i++] = (int8_t)(snr * 4);
    out_frame[i++] = (int8_t)(rssi);
    memcpy(&out_frame[i], log_raw, log_len);
    i += log_len;

    _serial->writeFrame(out_frame, i);
  }
}

bool MyMesh::isAutoAddEnabled() const {
  return (_prefs.manual_add_contacts & 1) == 0;
}

bool MyMesh::shouldAutoAddContactType(uint8_t contact_type) const {
  if ((_prefs.manual_add_contacts & 1) == 0) {
    return true;
  }

  uint8_t type_bit = 0;
  switch (contact_type) {
    case ADV_TYPE_CHAT:
      type_bit = AUTO_ADD_CHAT;
      break;
    case ADV_TYPE_REPEATER:
      type_bit = AUTO_ADD_REPEATER;
      break;
    case ADV_TYPE_ROOM:
      type_bit = AUTO_ADD_ROOM_SERVER;
      break;
    case ADV_TYPE_SENSOR:
      type_bit = AUTO_ADD_SENSOR;
      break;
    default:
      return false;  // Unknown type, don't auto-add
  }

  return (_prefs.autoadd_config & type_bit) != 0;
}

bool MyMesh::shouldOverwriteWhenFull() const {
  return (_prefs.autoadd_config & AUTO_ADD_OVERWRITE_OLDEST) != 0;
}

uint8_t MyMesh::getAutoAddMaxHops() const {
  return _prefs.autoadd_max_hops;
}

void MyMesh::onContactOverwrite(const uint8_t* pub_key) {
    _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE); // delete from storage
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACT_DELETED;
    memcpy(&out_frame[1], pub_key, PUB_KEY_SIZE);
    _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
  }
}

void MyMesh::onContactsFull() {
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACTS_FULL;
    _serial->writeFrame(out_frame, 1);
  }
}

void MyMesh::onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) {
  if (_serial->isConnected()) {
    if (is_new) {
      writeContactRespFrame(PUSH_CODE_NEW_ADVERT, contact);
    } else {
      out_frame[0] = PUSH_CODE_ADVERT;
      memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
      _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
    }
  } else {
#ifdef DISPLAY_CLASS
    if (_ui) _ui->notify(UIEventType::newContactMessage);
#endif
  }

  // add inbound-path to mem cache
  if (path && mesh::Packet::isValidPathLen(path_len)) {  // check path is valid
    AdvertPath* p = advert_paths;
    uint32_t oldest = 0xFFFFFFFF;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {   // check if already in table, otherwise evict oldest
      if (memcmp(advert_paths[i].pubkey_prefix, contact.id.pub_key, sizeof(AdvertPath::pubkey_prefix)) == 0) {
        p = &advert_paths[i];   // found
        break;
      }
      if (advert_paths[i].recv_timestamp < oldest) {
        oldest = advert_paths[i].recv_timestamp;
        p = &advert_paths[i];
      }
    }

    memcpy(p->pubkey_prefix, contact.id.pub_key, sizeof(p->pubkey_prefix));
    strcpy(p->name, contact.name);
    p->recv_timestamp = getRTCClock()->getCurrentTime();
    p->path_len = mesh::Packet::copyPath(p->path, path, path_len);
  }

  if (!is_new) dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY); // only schedule lazy write for contacts that are in contacts[]
}

static int sort_by_recent(const void *a, const void *b) {
  return ((AdvertPath *) b)->recv_timestamp - ((AdvertPath *) a)->recv_timestamp;
}

int MyMesh::getRecentlyHeard(AdvertPath dest[], int max_num) {
  if (max_num > ADVERT_PATH_TABLE_SIZE) max_num = ADVERT_PATH_TABLE_SIZE;
  qsort(advert_paths, ADVERT_PATH_TABLE_SIZE, sizeof(advert_paths[0]), sort_by_recent);

  for (int i = 0; i < max_num; i++) {
    dest[i] = advert_paths[i];
  }
  return max_num;
}

void MyMesh::onContactPathUpdated(const ContactInfo &contact) {
  out_frame[0] = PUSH_CODE_PATH_UPDATED;
  memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
  _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE); // NOTE: app may not be connected

  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
}

ContactInfo*  MyMesh::processAck(const uint8_t *data) {
  // see if matches any in a table
  for (int i = 0; i < EXPECTED_ACK_TABLE_SIZE; i++) {
    if (memcmp(data, &expected_ack_table[i].ack, 4) == 0) { // got an ACK from recipient
      out_frame[0] = PUSH_CODE_SEND_CONFIRMED;
      memcpy(&out_frame[1], data, 4);
      uint32_t trip_time = _ms->getMillis() - expected_ack_table[i].msg_sent;
      memcpy(&out_frame[5], &trip_time, 4);
      _serial->writeFrame(out_frame, 9);

      // NOTE: the same ACK can be received multiple times!
      expected_ack_table[i].ack = 0; // clear expected hash, now that we have received ACK
      return expected_ack_table[i].contact;
    }
  }
  return checkConnectionsAck(data);
}

void MyMesh::queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt,
                          uint32_t sender_timestamp, const uint8_t *extra, int extra_len, const char *text) {
  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV;
  }
  memcpy(&out_frame[i], from.id.pub_key, 6);
  i += 6; // just 6-byte prefix
  // The app gets the packed byte (low 6 bits = hop count, top 2 = hash size - 1),
  // same as upstream: it decodes both fields out of it. Our own display wants
  // just the hop count, or it renders "[64]" for a 2-byte-hash sender.
  out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  uint8_t path_len = pkt->isRouteFlood() ? pkt->getPathHashCount() : 0xFF;
  out_frame[i++] = txt_type;
  memcpy(&out_frame[i], &sender_timestamp, 4);
  i += 4;
  if (extra_len > 0) {
    memcpy(&out_frame[i], extra, extra_len);
    i += extra_len;
  }
  int tlen = strlen(text); // TODO: UTF-8 ??
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = MAX_FRAME_SIZE - i;
  }
  memcpy(&out_frame[i], text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }

#ifdef DISPLAY_CLASS
  // we only want to show text messages on display, not cli data
  bool should_display = txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN;
  if (should_display && _ui) {
    _ui->newMsg(path_len, from.name, text, offline_queue_len, true);
    if (!_serial->isConnected()) {
      _ui->notify(UIEventType::contactMessage);
    }
  }
#endif
}

void MyMesh::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                          const uint8_t* app_data, size_t app_data_len) {
  BaseChatMesh::onAdvertRecv(packet, id, timestamp, app_data, app_data_len);

  uint32_t pub_hash;
  memcpy(&pub_hash, id.pub_key, 4);
#ifdef WITH_COMPANION_CLI
  if (_ts_from_adverts)
#endif
  if (_ts.feedAdvert(timestamp, pub_hash)) {
    auto r = _ts.trySync(getRTCClock(), &sensors, hasRecentAppTimeSet());
    if (r == TsSyncResult::GPS) noteTimeSource(TIME_SOURCE_GPS);
    else if (r == TsSyncResult::CLOCK) noteTimeSource(TIME_SOURCE_ADVERT);
  }
}

bool MyMesh::filterRecvFloodPacket(mesh::Packet* packet) {
  // REVISIT: try to determine which Region (from transport_codes[1]) that Sender is indicating for replies/responses
  //    if unknown, fallback to finding Region from transport_codes[0], the 'scope' used by Sender
  return false;
}

bool MyMesh::allowPacketForward(const mesh::Packet* packet) {
  return _prefs.isRepeatEn();
}

void MyMesh::sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis) {
  if (scope.isNull()) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);
  } else {
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;  // REVISIT: set to 'home' Region, for sender/return region?
    sendFlood(pkt, codes, delay_millis, _prefs.path_hash_mode + 1);
  }
}

void MyMesh::sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: dynamic send_scope, depending on recipient and current 'home' Region
  if (send_unscoped) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);  // app has explicitly requested un-scoped
  } else {
    TransportKey default_scope;
    memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));

    auto scope = send_scope.isNull() ? &default_scope : &send_scope;
    sendFloodScoped(*scope, pkt, delay_millis);
  }
}
void MyMesh::sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: have per-channel send_scope
  if (send_unscoped) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);  // app has explicitly requested un-scoped
  } else {
    TransportKey default_scope;
    memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));

    auto scope = send_scope.isNull() ? &default_scope : &send_scope;
    sendFloodScoped(*scope, pkt, delay_millis);
  }
}

void MyMesh::onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const char *text) {
#ifdef WITH_COMPANION_CLI
  if (_ts_from_messages)
#endif
  if (_ts.feedMessage(sender_timestamp, getRTCClock()->getCurrentTime()) && !hasRecentAppTimeSet()) {
    auto r = _ts.trySync(getRTCClock(), &sensors, false);
    if (r == TsSyncResult::GPS) noteTimeSource(TIME_SOURCE_GPS);
    else if (r == TsSyncResult::CLOCK) noteTimeSource(TIME_SOURCE_ADVERT);
  }
#ifdef WITH_COMPANION_CLI
  MESH_DEBUG_PRINTLN("PM recv: cli=%s pin='%.8s' text='%.20s'", _cli ? "set" : "NULL", _cli_pin, text);
  if (_remote_cli_enabled && _cli_pin[0] && strlen(text) > 8 && strncasecmp(text, _cli_pin, 8) == 0 && text[8] == ' ') {
    handleRemoteCLI(from, sender_timestamp, text + 9);
    return;
  }
#endif
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_PLAIN, pkt, sender_timestamp, NULL, 0, text);
}

void MyMesh::onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                               const char *text) {
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_CLI_DATA, pkt, sender_timestamp, NULL, 0, text);
}

void MyMesh::onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                                 const uint8_t *sender_prefix, const char *text) {
  if (_ts.feedMessage(sender_timestamp, getRTCClock()->getCurrentTime()) && !hasRecentAppTimeSet()) {
    auto r = _ts.trySync(getRTCClock(), &sensors, false);
    if (r == TsSyncResult::GPS) noteTimeSource(TIME_SOURCE_GPS);
    else if (r == TsSyncResult::CLOCK) noteTimeSource(TIME_SOURCE_ADVERT);
  }
  markConnectionActive(from);
  // from.sync_since change needs to be persisted
  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  queueMessage(from, TXT_TYPE_SIGNED_PLAIN, pkt, sender_timestamp, sender_prefix, 4, text);
}

void MyMesh::onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                                  const char *text) {
  if (_ts.feedMessage(timestamp, getRTCClock()->getCurrentTime()) && !hasRecentAppTimeSet()) {
    auto r = _ts.trySync(getRTCClock(), &sensors, false);
    if (r == TsSyncResult::GPS) noteTimeSource(TIME_SOURCE_GPS);
    else if (r == TsSyncResult::CLOCK) noteTimeSource(TIME_SOURCE_ADVERT);
  }
  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV;
  }

  uint8_t channel_idx = findChannelIdx(channel);
  {
    ChannelDetails ch;
    if (getChannel(channel_idx, ch) && strcmp(ch.name, "TerminalCLI") == 0) return;
  }
  out_frame[i++] = channel_idx;
  // The app gets the packed byte (low 6 bits = hop count, top 2 = hash size - 1),
  // same as upstream: it decodes both fields out of it. Our own display wants
  // just the hop count, or it renders "[64]" for a 2-byte-hash sender.
  out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  uint8_t path_len = pkt->isRouteFlood() ? pkt->getPathHashCount() : 0xFF;

  out_frame[i++] = TXT_TYPE_PLAIN;
  memcpy(&out_frame[i], &timestamp, 4);
  i += 4;
  int tlen = strlen(text); // TODO: UTF-8 ??
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = MAX_FRAME_SIZE - i;
  }
  memcpy(&out_frame[i], text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  } else {
#ifdef DISPLAY_CLASS
    if (_ui) _ui->notify(UIEventType::channelMessage);
#endif
  }
#ifdef DISPLAY_CLASS
  // Get the channel name from the channel index
  const char *channel_name = "Unknown";
  ChannelDetails channel_details;
  if (getChannel(channel_idx, channel_details)) {
    channel_name = channel_details.name;
  }
  if (_ui) _ui->newMsg(path_len, channel_name, text, offline_queue_len);
#endif
}

void MyMesh::onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                               const uint8_t *data, size_t data_len) {
  if (data_len > MAX_CHANNEL_DATA_LENGTH) {
    MESH_DEBUG_PRINTLN("onChannelDataRecv: dropping payload_len=%d exceeds frame limit=%d",
                       (uint32_t)data_len, (uint32_t)MAX_CHANNEL_DATA_LENGTH);
    return;
  }

  int i = 0;
  out_frame[i++] = RESP_CODE_CHANNEL_DATA_RECV;
  out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
  out_frame[i++] = 0; // reserved1
  out_frame[i++] = 0; // reserved2

  uint8_t channel_idx = findChannelIdx(channel);
  out_frame[i++] = channel_idx;
  out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;  // packed byte, as above
  out_frame[i++] = (uint8_t)(data_type & 0xFF);
  out_frame[i++] = (uint8_t)(data_type >> 8);
  out_frame[i++] = (uint8_t)data_len;

  int copy_len = (int)data_len;
  if (copy_len > 0) {
    memcpy(&out_frame[i], data, copy_len);
    i += copy_len;
  }
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }
}

uint8_t MyMesh::onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                                 uint8_t len, uint8_t *reply) {
  if (data[0] == REQ_TYPE_GET_TELEMETRY_DATA) {
    uint8_t permissions = 0;
    uint8_t cp = contact.flags >> 1; // LSB used as 'favourite' bit (so only use upper bits)

    if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_ALL) {
      permissions = TELEM_PERM_BASE;
    } else if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_FLAGS) {
      permissions = cp & TELEM_PERM_BASE;
    }

    if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_LOCATION;
    } else if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_LOCATION;
    }

    if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_ENVIRONMENT;
    } else if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_ENVIRONMENT;
    }

    uint8_t perm_mask = ~(data[1]);    // NEW: first reserved byte (of 4), is now inverse mask to apply to permissions
    permissions &= perm_mask;

    if (permissions & TELEM_PERM_BASE) { // only respond if base permission bit is set
      telemetry.reset();
      telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      // query other sensors -- target specific
      sensors.querySensors(permissions, telemetry);

      float temperature = board.getMCUTemperature();
      if(!isnan(temperature)) { // Supported boards with built-in temperature sensor. ESP32-C3 may return NAN
        telemetry.addTemperature(TELEM_CHANNEL_SELF, temperature); // Built-in MCU Temperature
      }

      memcpy(reply, &sender_timestamp,
             4); // reflect sender_timestamp back in response packet (kind of like a 'tag')

      uint8_t tlen = telemetry.getSize();
      memcpy(&reply[4], telemetry.getBuffer(), tlen);
      return 4 + tlen;
    }
  }
  return 0; // unknown
}

void MyMesh::onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) {
  uint32_t tag;
  memcpy(&tag, data, 4);

  if (pending_login && memcmp(&pending_login, contact.id.pub_key, 4) == 0) { // check for login response
    // yes, is response to pending sendLogin()
    pending_login = 0;

    int i = 0;
    if (memcmp(&data[4], "OK", 2) == 0) { // legacy Repeater login OK response
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = 0; // legacy: is_admin = false
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6;                                     // pub_key_prefix
    } else if (data[4] == RESP_SERVER_LOGIN_OK) { // new login response
      uint16_t keep_alive_secs = ((uint16_t)data[5]) * 16;
      if (keep_alive_secs > 0) {
        startConnection(contact, keep_alive_secs);
      }
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = data[6]; // permissions (eg. is_admin)
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
      memcpy(&out_frame[i], &tag, 4);
      i += 4; // NEW: include server timestamp
      out_frame[i++] = data[7]; // NEW (v7): ACL permissions
      out_frame[i++] = data[12]; // FIRMWARE_VER_LEVEL
    } else {
      out_frame[i++] = PUSH_CODE_LOGIN_FAIL;
      out_frame[i++] = 0; // reserved
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
    }
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && // check for status response
             pending_status &&
             memcmp(&pending_status, contact.id.pub_key, 4) == 0 // legacy matching scheme
                                                                 // FUTURE: tag == pending_status
  ) {
    pending_status = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_STATUS_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && tag == pending_telemetry) {  // check for matching response tag
    pending_telemetry = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && tag == pending_req) {  // check for matching response tag
    pending_req = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_BINARY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], &tag, 4);   // app needs to match this to RESP_CODE_SENT.tag
    i += 4;
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  }
}

bool MyMesh::onContactPathRecv(ContactInfo& contact, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) {
  if (extra_type == PAYLOAD_TYPE_RESPONSE && extra_len > 4) {
    uint32_t tag;
    memcpy(&tag, extra, 4);

    if (tag == pending_discovery) {  // check for matching response tag)
      pending_discovery = 0;

      if (!mesh::Packet::isValidPathLen(in_path_len) || !mesh::Packet::isValidPathLen(out_path_len)) {
        MESH_DEBUG_PRINTLN("onContactPathRecv, invalid path sizes: %d, %d", in_path_len, out_path_len);
      } else {
        int i = 0;
        out_frame[i++] = PUSH_CODE_PATH_DISCOVERY_RESPONSE;
        out_frame[i++] = 0; // reserved
        memcpy(&out_frame[i], contact.id.pub_key, 6);
        i += 6; // pub_key_prefix
        out_frame[i++] = out_path_len;
        i += mesh::Packet::writePath(&out_frame[i], out_path, out_path_len);
        out_frame[i++] = in_path_len;
        i += mesh::Packet::writePath(&out_frame[i], in_path, in_path_len);
        // NOTE: telemetry data in 'extra' is discarded at present

        _serial->writeFrame(out_frame, i);
      }
      return false;  // DON'T send reciprocal path!
    }
  }
  // let base class handle received path and data
  return BaseChatMesh::onContactPathRecv(contact, in_path, in_path_len, out_path, out_path_len, extra_type, extra, extra_len);
}

void MyMesh::onControlDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_CONTROL_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = packet->path_len;
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), data received while app offline");
  }
}

void MyMesh::onRawDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_RAW_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = 0xFF; // reserved (possibly path_len in future)
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), data received while app offline");
  }
}

void MyMesh::onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                         const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) {
  uint8_t path_sz = flags & 0x03;  // NEW v1.11+
  if (12 + path_len + (path_len >> path_sz) + 1 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onTraceRecv(), path_len is too long: %d", (uint32_t)path_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_TRACE_DATA;
  out_frame[i++] = 0; // reserved
  out_frame[i++] = path_len;
  out_frame[i++] = flags;
  memcpy(&out_frame[i], &tag, 4);
  i += 4;
  memcpy(&out_frame[i], &auth_code, 4);
  i += 4;
  memcpy(&out_frame[i], path_hashes, path_len);
  i += path_len;

  memcpy(&out_frame[i], path_snrs, path_len >> path_sz);
  i += path_len >> path_sz;
  out_frame[i++] = (int8_t)(packet->getSNR() * 4); // extra/final SNR (to this node)

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onTraceRecv(), data received while app offline");
  }
}

uint32_t MyMesh::calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const {
  return SEND_TIMEOUT_BASE_MILLIS + (FLOOD_SEND_TIMEOUT_FACTOR * pkt_airtime_millis);
}
uint32_t MyMesh::calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const {
  uint8_t path_hash_count = path_len & 63;
  return SEND_TIMEOUT_BASE_MILLIS +
         ((pkt_airtime_millis * DIRECT_SEND_PERHOP_FACTOR + DIRECT_SEND_PERHOP_EXTRA_MILLIS) *
          (path_hash_count + 1));
}

void MyMesh::onSendTimeout() {}

MyMesh::MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui)
    : BaseChatMesh(radio, *new ArduinoMillis(), rng, rtc, *new StaticPoolPacketManager(16), tables),
      _serial(NULL), telemetry(MAX_PACKET_PAYLOAD - 4), _store(&store), _ui(ui), _iter(0) {
  _iter_started = false;
  _cli_rescue = false;
  offline_queue_len = 0;
  app_target_ver = 0;
  clearPendingReqs();
  next_ack_idx = 0;
  sign_data = NULL;
  dirty_contacts_expiry = 0;
  dirty_prefs_expiry = 0;
  memset(advert_paths, 0, sizeof(advert_paths));
  memset(send_scope.key, 0, sizeof(send_scope.key));
  send_unscoped = false;

  // defaults
  _prefs.airtime_factor = 1.0;
  strcpy(_prefs.node_name, "NONAME");
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.gps_enabled = 0;       // GPS disabled by default
  _prefs.gps_interval = 0;      // No automatic GPS updates by default
  _prefs.ui_pm_clock_mode = 1;  // PM inline on clock page by default
  _prefs.ui_clock_dim_mode = 0; // normal auto-off by default
  _prefs.adc_multiplier = 0.0f; // 0.0f = use board default
  _prefs.ui_display_rotation = 3;   // default landscape (matches compile-time DISPLAY_ROTATION=3)
  _prefs.ui_max_unread_idx = 1;     // 32 unread messages
  _prefs.ui_max_log_idx = 1;        // 32 history messages
  _prefs.ui_charge_uptime_base = 0;
  //_prefs.rx_delay_base = 10.0f;  enable once new algo fixed
  _prefs.setRepeatEn(false);
#if defined(USE_SX1262) || defined(USE_SX1268)
#ifdef SX126X_RX_BOOSTED_GAIN
  _prefs.rx_boosted_gain = SX126X_RX_BOOSTED_GAIN;
#else
  _prefs.rx_boosted_gain = 1; // enabled by default
#endif
#endif
}

void MyMesh::noteTimeSource(TimeSource source) {
  _time_source = source;
#ifdef DISPLAY_CLASS
  if (_ui) _ui->notify();
#endif
}

bool MyMesh::hasRecentAppTimeSet() const {
  uint32_t now = getRTCClock()->getCurrentTime();
  return _app_time_lock_until != 0 && now < _app_time_lock_until;
}

const char *MyMesh::getTimeSourceLabel() const {
  switch (_time_source) {
    case TIME_SOURCE_CONTACTS: return "CNT";
    case TIME_SOURCE_ADVERT:   return "ADV";
    case TIME_SOURCE_APP:      return "APP";
    case TIME_SOURCE_RTC:      return "RTC";
    case TIME_SOURCE_GPS:      return "GPS";
    case TIME_SOURCE_UNSET:
    default:                   return "---";
  }
}

void MyMesh::begin(bool has_display) {
  BaseChatMesh::begin();
  noteTimeSource(getRTCClock()->isTimeReliable() ? TIME_SOURCE_RTC : TIME_SOURCE_UNSET);

  if (!_store->loadMainIdentity(self_id)) {
    self_id = radio_new_identity(); // create new random identity
    int count = 0;
    while (count < 10 && (self_id.pub_key[0] == 0x00 || self_id.pub_key[0] == 0xFF)) { // reserved id hashes
      self_id = radio_new_identity();
      count++;
    }
    _store->saveMainIdentity(self_id);
  }

// if name is provided as a build flag, use that as default node name instead
#ifdef ADVERT_NAME
  strcpy(_prefs.node_name, ADVERT_NAME);
#else
  // use hex of first 4 bytes of identity public key as default node name
  char pub_key_hex[10];
  mesh::Utils::toHex(pub_key_hex, self_id.pub_key, 4);
  strcpy(_prefs.node_name, pub_key_hex);
#endif

  // if build provides default-scope, init with that
#ifdef DEFAULT_FLOOD_SCOPE_NAME
  strcpy(_prefs.default_scope_name, DEFAULT_FLOOD_SCOPE_NAME);
  {
    TransportKeyStore temp;
    TransportKey key;
    temp.getAutoKeyFor(0, "#" DEFAULT_FLOOD_SCOPE_NAME, key);
    memcpy(_prefs.default_scope_key, key.key, sizeof(key.key));
  }
#endif

  // load persisted prefs
  _store->loadPrefs(_prefs);
  sensors.node_lat = _prefs.node_lat;
  sensors.node_lon = _prefs.node_lon;

  // sanitise bad pref values
  _prefs.rx_delay_base = constrain(_prefs.rx_delay_base, 0, 20.0f);
  _prefs.airtime_factor = constrain(_prefs.airtime_factor, 0, 9.0f);
  _prefs.freq = constrain(_prefs.freq, 150.0f, 2500.0f);
  _prefs.bw = constrain(_prefs.bw, 7.8f, 500.0f);
  _prefs.sf = constrain(_prefs.sf, 5, 12);
  _prefs.cr = constrain(_prefs.cr, 5, 8);
  _prefs.tx_power_dbm = constrain(_prefs.tx_power_dbm, -9, MAX_LORA_TX_POWER);
  _prefs.gps_enabled = constrain(_prefs.gps_enabled, 0, 1);  // Ensure boolean 0 or 1
  _prefs.gps_interval = constrain(_prefs.gps_interval, 0, 86400);  // Max 24 hours
  _prefs.cyr2lat_channels = constrain(_prefs.cyr2lat_channels, 0, 1);
  _prefs.cyr2lat_contacts = constrain(_prefs.cyr2lat_contacts, 0, 1);
  _prefs.ui_pm_clock_mode = constrain(_prefs.ui_pm_clock_mode, 0, 1);
  _prefs.ui_clock_dim_mode = constrain(_prefs.ui_clock_dim_mode, 0, 1);
  _prefs.adc_multiplier = constrain(_prefs.adc_multiplier, 0.0f, 10.0f);
  board.setAdcMultiplier(_prefs.adc_multiplier);
  _prefs.ui_display_rotation = constrain(_prefs.ui_display_rotation, 0, 3);
  _prefs.ui_max_unread_idx = constrain(_prefs.ui_max_unread_idx, 0, 2);
  _prefs.ui_max_log_idx = constrain(_prefs.ui_max_log_idx, 0, 2);
  _cyr2lat_channels_enabled = _prefs.cyr2lat_channels != 0;
  _cyr2lat_contacts_enabled = _prefs.cyr2lat_contacts != 0;
#ifdef BLE_PIN_CODE // 123456 by default
  if (_prefs.ble_pin == 0) {
#ifdef DISPLAY_CLASS
    if (has_display && BLE_PIN_CODE == 123456) {
      StdRNG rng;
      _active_ble_pin = rng.nextInt(100000, 999999); // random pin each session
    } else {
      _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
    }
#else
    _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
#endif
  } else {
    _active_ble_pin = _prefs.ble_pin;
  }
#else
  _active_ble_pin = 0;
#endif

  resetContacts();
  _store->loadContacts(this);
  uint32_t rtc_before_contacts = getRTCClock()->getCurrentTime();
  bootstrapRTCfromContacts();
  uint32_t rtc_after_contacts = getRTCClock()->getCurrentTime();
  if (!getRTCClock()->isTimeReliable() && rtc_after_contacts > rtc_before_contacts &&
      rtc_after_contacts > MIN_VALID_TS && rtc_after_contacts < MAX_VALID_TS) {
    noteTimeSource(TIME_SOURCE_CONTACTS);
  }
  addChannel("Public", PUBLIC_GROUP_PSK); // pre-configure Andy's public channel
  _store->loadChannels(this);

  radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
  radio_driver.setTxPower(_prefs.tx_power_dbm);
  radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);
  MESH_DEBUG_PRINTLN("RX Boosted Gain Mode: %s",
                     radio_driver.getRxBoostedGainMode() ? "Enabled" : "Disabled");

#ifdef WITH_COMPANION_CLI
  _cli_cb = new CompanionCLICallbacks(*this, *_store);
  _cli    = new CommonCLI(board, *getRTCClock(), sensors, &_prefs, _cli_cb);
  { // PIN from last 4 bytes of private key — not derivable from public info
    uint8_t prv[PRV_KEY_SIZE];
    self_id.writeTo(prv, PRV_KEY_SIZE);
    mesh::Utils::toHex(_cli_pin, prv + PRV_KEY_SIZE - 4, 4);
  }
  Serial.printf("CLI PIN: %s\n", _cli_pin);

  // auto-register TerminalCLI channel if not present
  {
    bool found = false;
    ChannelDetails ch;
    for (int i = 0; i < MAX_GROUP_CHANNELS && !found; i++) {
      if (getChannel(i, ch) && ch.name[0] && strcmp(ch.name, "TerminalCLI") == 0)
        found = true;
    }
    if (found) {
      MESH_DEBUG_PRINTLN("CLI: TerminalCLI channel already present");
    } else if (addChannel("TerminalCLI", TERMINAL_CLI_PSK)) {
      MESH_DEBUG_PRINTLN("CLI: TerminalCLI channel registered");
      saveChannels();
    } else {
      MESH_DEBUG_PRINTLN("CLI: ERROR — addChannel(TerminalCLI) failed (slots full or bad PSK)");
    }
  }
#endif
}

const char *MyMesh::getNodeName() {
  return _prefs.node_name;
}
NodePrefs *MyMesh::getNodePrefs() {
  return &_prefs;
}
uint32_t MyMesh::getBLEPin() {
  return _active_ble_pin;
}

void MyMesh::deferSavePrefs() {
  dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
}

struct FreqRange {
  uint32_t lower_freq, upper_freq;
};

static FreqRange repeat_freq_ranges[] = {
  #ifdef ALLOWED_REPEAT_FREQ_RANGE
  ALLOWED_REPEAT_FREQ_RANGE
  #else
  { 433000, 433000 },
  { 869495, 869495 },
  { 918000, 918000 }
  #endif
};

bool MyMesh::isValidClientRepeatFreq(uint32_t f) const {
  for (int i = 0; i < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]); i++) {
    auto r = &repeat_freq_ranges[i];
    if (f >= r->lower_freq && f <= r->upper_freq) return true;
  }
  return false;
}

void MyMesh::startInterface(BaseSerialInterface &serial) {
  _serial = &serial;
  serial.enable();
}

void MyMesh::handleCmdFrame(size_t len) {
  if (cmd_frame[0] == CMD_DEVICE_QUERY && len >= 2) { // sent when app establishes connection
    app_target_ver = cmd_frame[1];                    // which version of protocol does app understand

    int i = 0;
    out_frame[i++] = RESP_CODE_DEVICE_INFO;
    out_frame[i++] = FIRMWARE_VER_CODE;
    out_frame[i++] = MAX_CONTACTS / 2;   // v3+
    out_frame[i++] = MAX_GROUP_CHANNELS; // v3+
    memcpy(&out_frame[i], &_prefs.ble_pin, 4);
    i += 4;
    memset(&out_frame[i], 0, 12);
    strcpy((char *)&out_frame[i], FIRMWARE_BUILD_DATE);
    i += 12;
    StrHelper::strzcpy((char *)&out_frame[i], board.getManufacturerName(), 40);
    i += 40;
    StrHelper::strzcpy((char *)&out_frame[i], FIRMWARE_VERSION, 20);
    i += 20;
    out_frame[i++] = _prefs.isRepeatEn() ? 1 : 0;   // v9+
    out_frame[i++] = _prefs.path_hash_mode;  // v10+
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_APP_START &&
             len >= 8) { // sent when app establishes connection, respond with node ID
    //  cmd_frame[1..7]  reserved future
    char *app_name = (char *)&cmd_frame[8];
    cmd_frame[len] = 0; // make app_name null terminated
    MESH_DEBUG_PRINTLN("App %s connected", app_name);

    _iter_started = false; // stop any left-over ContactsIterator
    int i = 0;
    out_frame[i++] = RESP_CODE_SELF_INFO;
    out_frame[i++] = ADV_TYPE_CHAT; // what this node Advert identifies as (maybe node's pronouns too?? :-)
    out_frame[i++] = _prefs.tx_power_dbm;
    out_frame[i++] = MAX_LORA_TX_POWER;
    memcpy(&out_frame[i], self_id.pub_key, PUB_KEY_SIZE);
    i += PUB_KEY_SIZE;

    int32_t lat, lon;
    lat = (sensors.node_lat * 1000000.0);
    lon = (sensors.node_lon * 1000000.0);
    memcpy(&out_frame[i], &lat, 4);
    i += 4;
    memcpy(&out_frame[i], &lon, 4);
    i += 4;
    out_frame[i++] = _prefs.multi_acks; // new v7+
    out_frame[i++] = _prefs.advert_loc_policy;
    out_frame[i++] = (_prefs.telemetry_mode_env << 4) | (_prefs.telemetry_mode_loc << 2) |
                     (_prefs.telemetry_mode_base); // v5+
    out_frame[i++] = _prefs.manual_add_contacts;

    uint32_t freq = _prefs.freq * 1000;
    memcpy(&out_frame[i], &freq, 4);
    i += 4;
    uint32_t bw = _prefs.bw * 1000;
    memcpy(&out_frame[i], &bw, 4);
    i += 4;
    out_frame[i++] = _prefs.sf;
    out_frame[i++] = _prefs.cr;

    int tlen = strlen(_prefs.node_name); // revisit: UTF_8 ??
    memcpy(&out_frame[i], _prefs.node_name, tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_TXT_MSG && len >= 14) {
    int i = 1;
    uint8_t txt_type = cmd_frame[i++];
    uint8_t attempt = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    uint8_t *pub_key_prefix = &cmd_frame[i];
    i += 6;
    ContactInfo *recipient = lookupContactByPubKey(pub_key_prefix, 6);
    if (recipient && (txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_CLI_DATA)) {
      char *text = (char *)&cmd_frame[i];
      int tlen = len - i;
      uint32_t est_timeout;
      text[tlen] = 0; // ensure null
      int result;
      uint32_t expected_ack;
      if (txt_type == TXT_TYPE_CLI_DATA) {
        msg_timestamp = getRTCClock()->getCurrentTimeUnique(); // Use node's RTC instead of app timestamp to avoid tripping replay protection
        result = sendCommandData(*recipient, msg_timestamp, attempt, text, est_timeout);
        expected_ack = 0; // no Ack expected
      } else {
        const char* send_text = text;
        char text_buf[MAX_FRAME_SIZE + 1];
        if (_cyr2lat_contacts_enabled) {
          transliterateChannelText(text, tlen, text_buf, sizeof(text_buf));
          send_text = text_buf;
        }
        result = sendMessage(*recipient, msg_timestamp, attempt, send_text, expected_ack, est_timeout);
      }
      // TODO: add expected ACK to table
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        if (expected_ack) {
          expected_ack_table[next_ack_idx].msg_sent = _ms->getMillis(); // add to circular table
          expected_ack_table[next_ack_idx].ack = expected_ack;
          expected_ack_table[next_ack_idx].contact = recipient;
          next_ack_idx = (next_ack_idx + 1) % EXPECTED_ACK_TABLE_SIZE;
        }

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &expected_ack, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(recipient == NULL
                        ? ERR_CODE_NOT_FOUND
                        : ERR_CODE_UNSUPPORTED_CMD); // unknown recipient, or unsupported TXT_TYPE_*
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_TXT_MSG) { // send GroupChannel text msg
    int i = 1;
    uint8_t txt_type = cmd_frame[i++]; // should be TXT_TYPE_PLAIN
    uint8_t channel_idx = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    const char *text = (char *)&cmd_frame[i];

    if (txt_type != TXT_TYPE_PLAIN) {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    } else {
      ChannelDetails channel;
      bool success = getChannel(channel_idx, channel);
      if (success && strcmp(channel.name, "TerminalCLI") == 0) {
#ifdef WITH_COMPANION_CLI
        const_cast<char*>(text)[len - i] = '\0'; // text is not null-terminated in this frame type
        if (_ui) _ui->newOutgoingMsg(channel.name, text, false);
        handleTerminalCLI(channel_idx, msg_timestamp, text, true, true);
#else
        writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
#endif
      } else if (success) {
        const char* send_text = text;
        int send_len = len - i;
        int orig_len = send_len;
        char text_buf[MAX_FRAME_SIZE + 1];
        bool transformed = false;

        if (_cyr2lat_channels_enabled) {
          send_len = transliterateChannelText(text, send_len, text_buf, sizeof(text_buf));
          send_text = text_buf;
          transformed = textDiffers(text, orig_len, send_text, send_len);
        }
        int prefix_len = strlen(_prefs.node_name) + 2; // "<sender>: "
        if (send_len + prefix_len > MAX_TEXT_LEN) {
          send_len = MAX_TEXT_LEN - prefix_len;
          if (send_len < 0) send_len = 0;
        }
        if (send_text != text_buf) {
          int copy_n = send_len < (int)sizeof(text_buf) - 1 ? send_len : (int)sizeof(text_buf) - 1;
          memcpy(text_buf, send_text, copy_n);
          send_text = text_buf;
          send_len  = copy_n;
        }
        text_buf[send_len] = '\0';
        if (sendGroupMessageWithCyr2LatMap(msg_timestamp, channel.channel, _prefs.node_name, send_text, send_len,
                                           text, orig_len, transformed)) {
          if (_ui) _ui->newOutgoingMsg(channel.name, send_text, false);
          writeOKFrame();
        } else {
          writeErrFrame(ERR_CODE_NOT_FOUND);
        }
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
      }
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_DATA) { // send GroupChannel datagram
    if (len < 4) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }
    int i = 1;
    uint8_t channel_idx = cmd_frame[i++];
    uint8_t path_len = cmd_frame[i++];

    // validate path len, allowing 0xFF for flood
    if (!mesh::Packet::isValidPathLen(path_len) && path_len != OUT_PATH_UNKNOWN) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA invalid path size: %d", path_len);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }

    // parse provided path if not flood
    uint8_t path[MAX_PATH_SIZE];
    if (path_len != OUT_PATH_UNKNOWN) {
      i += mesh::Packet::writePath(path, &cmd_frame[i], path_len);
    }

    uint16_t data_type = ((uint16_t)cmd_frame[i]) | (((uint16_t)cmd_frame[i + 1]) << 8);
    i += 2;
    const uint8_t *payload = &cmd_frame[i];
    int payload_len = (len > (size_t)i) ? (int)(len - i) : 0;

    ChannelDetails channel;
    if (!getChannel(channel_idx, channel)) {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    } else if (data_type == DATA_TYPE_RESERVED) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (payload_len > MAX_CHANNEL_DATA_LENGTH) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA payload too long: %d > %d", payload_len, MAX_CHANNEL_DATA_LENGTH);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (sendGroupData(channel.channel, path, path_len, data_type, payload, payload_len)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACTS) { // get Contact list
    if (_iter_started) {
      writeErrFrame(ERR_CODE_BAD_STATE); // iterator is currently busy
    } else {
      if (len >= 5) { // has optional 'since' param
        memcpy(&_iter_filter_since, &cmd_frame[1], 4);
      } else {
        _iter_filter_since = 0;
      }

      uint8_t reply[5];
      reply[0] = RESP_CODE_CONTACTS_START;
      uint32_t count = getNumContacts(); // total, NOT filtered count
      memcpy(&reply[1], &count, 4);
      _serial->writeFrame(reply, 5);

      // start iterator
      _iter = startContactsIterator();
      _iter_started = true;
      _most_recent_lastmod = 0;
    }
  } else if (cmd_frame[0] == CMD_SET_ADVERT_NAME && len >= 2) {
    int nlen = len - 1;
    if (nlen > sizeof(_prefs.node_name) - 1) nlen = sizeof(_prefs.node_name) - 1; // max len
    memcpy(_prefs.node_name, &cmd_frame[1], nlen);
    _prefs.node_name[nlen] = 0; // null terminator
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_ADVERT_LATLON && len >= 9) {
    int32_t lat, lon, alt = 0;
    memcpy(&lat, &cmd_frame[1], 4);
    memcpy(&lon, &cmd_frame[5], 4);
    if (len >= 13) {
      memcpy(&alt, &cmd_frame[9], 4); // for FUTURE support
    }
    if (lat <= 90 * 1E6 && lat >= -90 * 1E6 && lon <= 180 * 1E6 && lon >= -180 * 1E6) {
      sensors.node_lat = ((double)lat) / 1000000.0;
      sensors.node_lon = ((double)lon) / 1000000.0;
      dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid geo coordinate
    }
  } else if (cmd_frame[0] == CMD_GET_DEVICE_TIME) {
    uint8_t reply[5];
    reply[0] = RESP_CODE_CURR_TIME;
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply[1], &now, 4);
    _serial->writeFrame(reply, 5);
  } else if (cmd_frame[0] == CMD_SET_DEVICE_TIME && len >= 5) {
    uint32_t secs;
    memcpy(&secs, &cmd_frame[1], 4);
    uint32_t curr = getRTCClock()->getCurrentTime();
    if (secs >= curr) {
      getRTCClock()->setCurrentTime(secs);
      _app_time_lock_until = secs + APP_TIME_HOLDOFF_SECS;
      noteTimeSource(TIME_SOURCE_APP);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SEND_SELF_ADVERT) {
    mesh::Packet* pkt;
    if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
      pkt = createSelfAdvert(_prefs.node_name);
    } else {
      pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
    }
    if (pkt) {
      if (len >= 2 && cmd_frame[1] == 1) { // optional param (1 = flood, 0 = zero hop)
        unsigned long delay_millis = 0;
        TransportKey default_scope;
        memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));
        sendFloodScoped(default_scope, pkt, delay_millis);
      } else {
        sendZeroHop(pkt);
      }
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_RESET_PATH && len >= 1 + 32) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      // recipient->lastmod = ??   shouldn't be needed, app already has this version of contact
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // unknown contact
    }
  } else if (cmd_frame[0] == CMD_ADD_UPDATE_CONTACT && len >= 1 + 32 + 2 + 1) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    uint32_t last_mod = getRTCClock()->getCurrentTime();  // fallback value if not present in cmd_frame
    if (recipient) {
      updateContactFromFrame(*recipient, last_mod, cmd_frame, len);
      recipient->lastmod = last_mod;
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      ContactInfo contact;
      updateContactFromFrame(contact, last_mod, cmd_frame, len);
      contact.lastmod = last_mod;
      contact.sync_since = 0;
      if (addContact(contact)) {
        dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_REMOVE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient && removeContact(*recipient)) {
      _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE);
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found, or unable to remove
    }
  } else if (cmd_frame[0] == CMD_SHARE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      if (shareContactZeroHop(*recipient)) {
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // unable to send
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACT_BY_KEY) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *contact = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (contact) {
      writeContactRespFrame(RESP_CODE_CONTACT, *contact);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found
    }
  } else if (cmd_frame[0] == CMD_EXPORT_CONTACT) {
    if (len < 1 + PUB_KEY_SIZE) {
      // export SELF
      mesh::Packet* pkt;
      if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
        pkt = createSelfAdvert(_prefs.node_name);
      } else {
        pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
      }
      if (pkt) {
        pkt->header |= ROUTE_TYPE_FLOOD; // would normally be sent in this mode

        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        uint8_t out_len = pkt->writeTo(&out_frame[1]);
        releasePacket(pkt); // undo the obtainNewPacket()
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // Error
      }
    } else {
      uint8_t *pub_key = &cmd_frame[1];
      ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
      uint8_t out_len;
      if (recipient && (out_len = exportContact(*recipient, &out_frame[1])) > 0) {
        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // not found
      }
    }
  } else if (cmd_frame[0] == CMD_IMPORT_CONTACT && len > 2 + 32 + 64) {
    if (importContact(&cmd_frame[1], len - 1)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SYNC_NEXT_MESSAGE) {
    int out_len;
    if ((out_len = getFromOfflineQueue(out_frame)) > 0) {
      _serial->writeFrame(out_frame, out_len);
#ifdef DISPLAY_CLASS
      if (_ui) _ui->msgRead(offline_queue_len);
#endif
    } else {
      out_frame[0] = RESP_CODE_NO_MORE_MESSAGES;
      _serial->writeFrame(out_frame, 1);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_PARAMS) {
    int i = 1;
    uint32_t freq;
    memcpy(&freq, &cmd_frame[i], 4);
    i += 4;
    uint32_t bw;
    memcpy(&bw, &cmd_frame[i], 4);
    i += 4;
    uint8_t sf = cmd_frame[i++];
    uint8_t cr = cmd_frame[i++];
    uint8_t repeat = 0;  // default - false
    if (len > i) {
      repeat = cmd_frame[i++];   // FIRMWARE_VER_CODE  9+
    }

    if (repeat && !isValidClientRepeatFreq(freq)) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (freq >= 150000 && freq <= 2500000 && sf >= 5 && sf <= 12 && cr >= 5 && cr <= 8 && bw >= 7000 &&
        bw <= 500000) {
      _prefs.sf = sf;
      _prefs.cr = cr;
      _prefs.freq = (float)freq / 1000.0;
      _prefs.bw = (float)bw / 1000.0;
      _prefs.setRepeatEn(repeat != 0);
      savePrefs();

      radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
      MESH_DEBUG_PRINTLN("OK: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);

      writeOKFrame();
    } else {
      MESH_DEBUG_PRINTLN("Error: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_TX_POWER) {
    int8_t power = (int8_t)cmd_frame[1];
    if (power < -9 || power > MAX_LORA_TX_POWER) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.tx_power_dbm = power;
      dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
      radio_driver.setTxPower(_prefs.tx_power_dbm);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SET_TUNING_PARAMS) {
    int i = 1;
    uint32_t rx, af;
    memcpy(&rx, &cmd_frame[i], 4);
    i += 4;
    memcpy(&af, &cmd_frame[i], 4);
    i += 4;
    _prefs.rx_delay_base = ((float)rx) / 1000.0f;
    _prefs.airtime_factor = ((float)af) / 1000.0f;
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_TUNING_PARAMS) {
    uint32_t rx = _prefs.rx_delay_base * 1000, af = _prefs.airtime_factor * 1000;
    int i = 0;
    out_frame[i++] = RESP_CODE_TUNING_PARAMS;
    memcpy(&out_frame[i], &rx, 4); i += 4;
    memcpy(&out_frame[i], &af, 4); i += 4;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SET_OTHER_PARAMS) {
    _prefs.manual_add_contacts = cmd_frame[1];
    if (len >= 3) {
      _prefs.telemetry_mode_base = cmd_frame[2] & 0x03; // v5+
      _prefs.telemetry_mode_loc = (cmd_frame[2] >> 2) & 0x03;
      _prefs.telemetry_mode_env = (cmd_frame[2] >> 4) & 0x03;

      if (len >= 4) {
        _prefs.advert_loc_policy = cmd_frame[3];
        if (len >= 5) {
          _prefs.multi_acks = cmd_frame[4];
        }
      }
    }
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_PATH_HASH_MODE && cmd_frame[1] == 0 && len >= 3) {
    if (cmd_frame[2] >= 3) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.path_hash_mode = cmd_frame[2];
      dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_REBOOT && memcmp(&cmd_frame[1], "reboot", 6) == 0) {
#ifdef NRF52_PLATFORM
    if (_serial && _serial->isConnected()) {
      // NRF52: can't write flash while BLE connected — defer, loop() will disconnect + save
      _pending_reboot_at = futureMillis(1500);
      _pending_reboot_deadline = futureMillis(30000);
    } else {
#else
    {
#endif
      if (dirty_contacts_expiry) { saveContacts(); dirty_contacts_expiry = 0; }
      if (dirty_prefs_expiry)    { savePrefs();    dirty_prefs_expiry    = 0; }
#ifdef ESP32
      SPIFFS.end();
#endif
      board.reboot();
    }
  } else if (cmd_frame[0] == CMD_GET_BATT_AND_STORAGE) {
    uint8_t reply[11];
    int i = 0;
    reply[i++] = RESP_CODE_BATT_AND_STORAGE;
    uint16_t battery_millivolts = board.getBattMilliVolts();
    uint32_t used = _store->getStorageUsedKb();
    uint32_t total = _store->getStorageTotalKb();
    memcpy(&reply[i], &battery_millivolts, 2); i += 2;
    memcpy(&reply[i], &used, 4); i += 4;
    memcpy(&reply[i], &total, 4); i += 4;
    _serial->writeFrame(reply, i);
  } else if (cmd_frame[0] == CMD_EXPORT_PRIVATE_KEY) {
#if ENABLE_PRIVATE_KEY_EXPORT
    uint8_t reply[65];
    reply[0] = RESP_CODE_PRIVATE_KEY;
    self_id.writeTo(&reply[1], 64);
    _serial->writeFrame(reply, 65);
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_IMPORT_PRIVATE_KEY && len >= 65) {
#if ENABLE_PRIVATE_KEY_IMPORT
    if (!mesh::LocalIdentity::validatePrivateKey(&cmd_frame[1])) {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid key
    } else {
        mesh::LocalIdentity identity;
        identity.readFrom(&cmd_frame[1], 64);
        if (_store->saveMainIdentity(identity)) {
          self_id = identity;
          writeOKFrame();
          // re-load contacts, to invalidate ecdh shared_secrets
          resetContacts();
          _store->loadContacts(this);
        } else {
          writeErrFrame(ERR_CODE_FILE_IO_ERROR);
        }
    }
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_SEND_RAW_DATA && len >= 6) {
    int i = 1;
    int8_t path_len = cmd_frame[i++];
    if (path_len >= 0 && i + path_len + 4 <= len) { // minimum 4 byte payload
      uint8_t *path = &cmd_frame[i];
      i += path_len;
      auto pkt = createRawData(&cmd_frame[i], len - i);
      if (pkt) {
        sendDirect(pkt, path, path_len);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    } else {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // flood, not supported (yet)
    }
  } else if (cmd_frame[0] == CMD_SEND_LOGIN && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    char *password = (char *)&cmd_frame[1 + PUB_KEY_SIZE];
    cmd_frame[len] = 0; // ensure null terminator in password
    if (recipient) {
      uint32_t est_timeout;
      int result = sendLogin(*recipient, password, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        memcpy(&pending_login, recipient->id.pub_key, 4); // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &pending_login, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_ANON_REQ && len > 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    ContactInfo anon;
    if (recipient == NULL) { // FIRMWARE_VER_CODE 13+,  allow non-contact requests
      memset(&anon, 0, sizeof(anon));
      memcpy(anon.id.pub_key, pub_key, PUB_KEY_SIZE);
      anon.out_path_len = 0;   // default to zero-hop direct
      anon.type = ADV_TYPE_NONE;  // unknown
      anon.lastmod = getRTCClock()->getCurrentTime();

      if (addContact(anon)) recipient = &anon;
    }
    uint8_t *data = &cmd_frame[1 + PUB_KEY_SIZE];
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendAnonReq(*recipient, data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL); // contacts full
    }
  } else if (cmd_frame[0] == CMD_SEND_STATUS_REQ && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_STATUS, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        // FUTURE:  pending_status = tag;  // match this in onContactResponse()
        memcpy(&pending_status, recipient->id.pub_key, 4); // legacy matching scheme
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_PATH_DISCOVERY_REQ && cmd_frame[1] == 0 && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[2];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      // 'Path Discovery' is just a special case of flood + Telemetry req
      uint8_t req_data[9];
      req_data[0] = REQ_TYPE_GET_TELEMETRY_DATA;
      req_data[1] = ~(TELEM_PERM_BASE);  // NEW: inverse permissions mask (ie. we only want BASE telemetry)
      memset(&req_data[2], 0, 3);  // reserved
      getRNG()->random(&req_data[5], 4);   // random blob to help make packet-hash unique
      auto save = recipient->out_path_len;    // temporarily force sendRequest() to flood
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      int result = sendRequest(*recipient, req_data, sizeof(req_data), tag, est_timeout);
      recipient->out_path_len = save;
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_discovery = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len >= 4 + PUB_KEY_SIZE) {  // can deprecate, in favour of CMD_SEND_BINARY_REQ
    uint8_t *pub_key = &cmd_frame[4];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_TELEMETRY_DATA, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_telemetry = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len == 4) {  // 'self' telemetry request
    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
    float temperature = board.getMCUTemperature();
    if(!isnan(temperature)) { // Supported boards with built-in temperature sensor. ESP32-C3 may return NAN
      telemetry.addTemperature(TELEM_CHANNEL_SELF, temperature); // Built-in MCU Temperature
    }

    // query other sensors -- target specific
    sensors.querySensors(0xFF, telemetry);

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], self_id.pub_key, 6);
    i += 6; // pub_key_prefix
    uint8_t tlen = telemetry.getSize();
    memcpy(&out_frame[i], telemetry.getBuffer(), tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_BINARY_REQ && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint8_t *req_data = &cmd_frame[1 + PUB_KEY_SIZE];
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, req_data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_HAS_CONNECTION && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    if (hasConnectionTo(pub_key)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_LOGOUT && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    stopConnection(pub_key);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_CHANNEL && len >= 2) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    if (getChannel(channel_idx, channel)) {
      int i = 0;
      out_frame[i++] = RESP_CODE_CHANNEL_INFO;
      out_frame[i++] = channel_idx;
      strcpy((char *)&out_frame[i], channel.name);
      i += 32;
      memcpy(&out_frame[i], channel.channel.secret, 16);
      i += 16; // NOTE: only 128-bit supported
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 32) {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // not supported (yet)
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 16) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    StrHelper::strncpy(channel.name, (char *)&cmd_frame[2], 32);
    memset(channel.channel.secret, 0, sizeof(channel.channel.secret));
    memcpy(channel.channel.secret, &cmd_frame[2 + 32], 16); // NOTE: only 128-bit supported
    if (setChannel(channel_idx, channel)) {
      saveChannels();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    }
  } else if (cmd_frame[0] == CMD_SIGN_START) {
    out_frame[0] = RESP_CODE_SIGN_START;
    out_frame[1] = 0; // reserved
    uint32_t len = MAX_SIGN_DATA_LEN;
    memcpy(&out_frame[2], &len, 4);
    _serial->writeFrame(out_frame, 6);

    if (sign_data) {
      free(sign_data);
    }
    sign_data = (uint8_t *)malloc(MAX_SIGN_DATA_LEN);
    sign_data_len = 0;
  } else if (cmd_frame[0] == CMD_SIGN_DATA && len > 1) {
    if (sign_data == NULL || sign_data_len + (len - 1) > MAX_SIGN_DATA_LEN) {
      writeErrFrame(sign_data == NULL ? ERR_CODE_BAD_STATE : ERR_CODE_TABLE_FULL); // error: too long
    } else {
      memcpy(&sign_data[sign_data_len], &cmd_frame[1], len - 1);
      sign_data_len += (len - 1);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SIGN_FINISH) {
    if (sign_data) {
      self_id.sign(&out_frame[1], sign_data, sign_data_len);

      free(sign_data); // don't need sign_data now
      sign_data = NULL;

      out_frame[0] = RESP_CODE_SIGNATURE;
      _serial->writeFrame(out_frame, 1 + SIGNATURE_SIZE);
    } else {
      writeErrFrame(ERR_CODE_BAD_STATE);
    }
  } else if (cmd_frame[0] == CMD_SEND_TRACE_PATH && len > 10 && len - 10 < MAX_PACKET_PAYLOAD-5) {
    uint8_t path_len = len - 10;
    uint8_t flags = cmd_frame[9];
    uint8_t path_sz = flags & 0x03;  // NEW v1.11+
    if ((path_len >> path_sz) > MAX_PATH_SIZE || (path_len % (1 << path_sz)) != 0) { // make sure is multiple of path_sz
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      uint32_t tag, auth;
      memcpy(&tag, &cmd_frame[1], 4);
      memcpy(&auth, &cmd_frame[5], 4);
      auto pkt = createTrace(tag, auth, flags);
      if (pkt) {
        sendDirect(pkt, &cmd_frame[10], path_len);

        uint32_t t = _radio->getEstAirtimeFor(pkt->payload_len + pkt->path_len + 2);
        uint32_t est_timeout = calcDirectTimeoutMillisFor(t, path_len >> path_sz);

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_SET_DEVICE_PIN && len >= 5) {

    // get pin from command frame
    uint32_t pin;
    memcpy(&pin, &cmd_frame[1], 4);

    // ensure pin is zero, or a valid 6 digit pin
    if (pin == 0 || (pin >= 100000 && pin <= 999999)) {
      _prefs.ble_pin = pin;
      if (pin != 0) _active_ble_pin = pin;  // don't zero out display — pin unchanged until reboot
      dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_CUSTOM_VARS) {
    out_frame[0] = RESP_CODE_CUSTOM_VARS;
    char *dp = (char *)&out_frame[1];
    for (int i = 0; i < sensors.getNumSettings() && dp - (char *)&out_frame[1] < 140; i++) {
      if (i > 0) {
        *dp++ = ',';
      }
      strcpy(dp, sensors.getSettingName(i));
      dp = strchr(dp, 0);
      *dp++ = ':';
      strcpy(dp, sensors.getSettingValue(i));
      dp = strchr(dp, 0);
    }
    _serial->writeFrame(out_frame, dp - (char *)out_frame);
  } else if (cmd_frame[0] == CMD_SET_CUSTOM_VAR && len >= 4) {
    cmd_frame[len] = 0;
    char *sp = (char *)&cmd_frame[1];
    char *np = strchr(sp, ':'); // look for separator char
    if (np) {
      *np++ = 0; // modify 'cmd_frame', replace ':' with null
      bool success = sensors.setSettingValue(sp, np);
      if (success) {
        #if ENV_INCLUDE_GPS == 1
        // Update node preferences for GPS settings
        if (strcmp(sp, "gps") == 0) {
          _prefs.gps_enabled = (np[0] == '1') ? 1 : 0;
          dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
        } else if (strcmp(sp, "gps_interval") == 0) {
          uint32_t interval_seconds = atoi(np);
          _prefs.gps_interval = constrain(interval_seconds, 0, 86400);
          dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
        }
        #endif
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_ADVERT_PATH && len >= PUB_KEY_SIZE+2) {
    // FUTURE use:  uint8_t reserved = cmd_frame[1];
    uint8_t *pub_key = &cmd_frame[2];
    AdvertPath* found = NULL;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {
      auto p = &advert_paths[i];
      if (memcmp(p->pubkey_prefix, pub_key, sizeof(p->pubkey_prefix)) == 0) {
        found = p;
        break;
      }
    }
    if (found) {
      int i = 0;
      out_frame[i++] = RESP_CODE_ADVERT_PATH;
      memcpy(&out_frame[i], &found->recv_timestamp, 4); i += 4;
      out_frame[i++] = found->path_len;
      i += mesh::Packet::writePath(&out_frame[i], found->path, found->path_len);
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_STATS && len >= 2) {
    uint8_t stats_type = cmd_frame[1];
    if (stats_type == STATS_TYPE_CORE) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_CORE;
      uint16_t battery_mv = board.getBattMilliVolts();
      uint32_t uptime_secs = _ms->getMillis() / 1000;
      uint8_t queue_len = (uint8_t)_mgr->getOutboundTotal();
      memcpy(&out_frame[i], &battery_mv, 2); i += 2;
      memcpy(&out_frame[i], &uptime_secs, 4); i += 4;
      memcpy(&out_frame[i], &_err_flags, 2); i += 2;
      out_frame[i++] = queue_len;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_RADIO) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_RADIO;
      int16_t noise_floor = (int16_t)_radio->getNoiseFloor();
      int8_t last_rssi = (int8_t)radio_driver.getLastRSSI();
      int8_t last_snr = (int8_t)(radio_driver.getLastSNR() * 4); // scaled by 4 for 0.25 dB precision
      uint32_t tx_air_secs = getTotalAirTime() / 1000;
      uint32_t rx_air_secs = getReceiveAirTime() / 1000;
      memcpy(&out_frame[i], &noise_floor, 2); i += 2;
      out_frame[i++] = last_rssi;
      out_frame[i++] = last_snr;
      memcpy(&out_frame[i], &tx_air_secs, 4); i += 4;
      memcpy(&out_frame[i], &rx_air_secs, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_PACKETS) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_PACKETS;
      uint32_t recv = radio_driver.getPacketsRecv();
      uint32_t sent = radio_driver.getPacketsSent();
      uint32_t n_sent_flood = getNumSentFlood();
      uint32_t n_sent_direct = getNumSentDirect();
      uint32_t n_recv_flood = getNumRecvFlood();
      uint32_t n_recv_direct = getNumRecvDirect();
      uint32_t n_recv_errors = radio_driver.getPacketsRecvErrors();
      memcpy(&out_frame[i], &recv, 4); i += 4;
      memcpy(&out_frame[i], &sent, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_errors, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid stats sub-type
    }
  } else if (cmd_frame[0] == CMD_FACTORY_RESET && memcmp(&cmd_frame[1], "reset", 5) == 0) {
    if (_serial) {
      MESH_DEBUG_PRINTLN("Factory reset: disabling serial interface to prevent reconnects (BLE/WiFi)");
      _serial->disable(); // Phone app disconnects before we can send OK frame so it's safe here
    }
    bool success = _store->formatFileSystem();
    if (success) {
      writeOKFrame();
      delay(1000);
      board.reboot();  // doesn't return
    } else {
      writeErrFrame(ERR_CODE_FILE_IO_ERROR);
    }
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 0) {
    if (len >= 2 + 16) {
      memcpy(send_scope.key, &cmd_frame[2], sizeof(send_scope.key));  // set scope override TransportKey
    } else {
      memset(send_scope.key, 0, sizeof(send_scope.key));  // reset scope override
    }
    send_unscoped = false;
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 1) {  // ver 12+
    send_unscoped = true;
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_DEFAULT_FLOOD_SCOPE && len >= 1) {
    if (len >= 1+31+16) {
      int n = strlen((char *) &cmd_frame[1]);
      if (n > 0 && n < 31) {
        strcpy(_prefs.default_scope_name, (char *) &cmd_frame[1]);
        memcpy(_prefs.default_scope_key, &cmd_frame[1+31], 16);
        dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      memset(_prefs.default_scope_name, 0, sizeof(_prefs.default_scope_name));  // set default scope to null
      memset(_prefs.default_scope_key, 0, sizeof(_prefs.default_scope_key));
      dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_GET_DEFAULT_FLOOD_SCOPE) {
    out_frame[0] = RESP_CODE_DEFAULT_FLOOD_SCOPE;
    if (strlen(_prefs.default_scope_name) > 0) {
      memcpy(&out_frame[1], _prefs.default_scope_name, 31);
      memcpy(&out_frame[1+31], _prefs.default_scope_key, 16);
      _serial->writeFrame(out_frame, 1+31+16);
    } else {
      _serial->writeFrame(out_frame, 1);   // no name or key means null
    }
  } else if (cmd_frame[0] == CMD_SEND_CONTROL_DATA && len >= 2 && (cmd_frame[1] & 0x80) != 0) {
    auto resp = createControlData(&cmd_frame[1], len - 1);
    if (resp) {
      sendZeroHop(resp);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_SET_AUTOADD_CONFIG) {
    _prefs.autoadd_config = cmd_frame[1];
    if (len >= 3) {
      _prefs.autoadd_max_hops = min(cmd_frame[2], (uint8_t)64);
    }
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_AUTOADD_CONFIG) {
    int i = 0;
    out_frame[i++] = RESP_CODE_AUTOADD_CONFIG;
    out_frame[i++] = _prefs.autoadd_config;
    out_frame[i++] = _prefs.autoadd_max_hops;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_GET_ALLOWED_REPEAT_FREQ) {
    int i = 0;
    out_frame[i++] = RESP_ALLOWED_REPEAT_FREQ;
    for (int k = 0; k < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]) && i + 8 < sizeof(out_frame); k++) {
      auto r = &repeat_freq_ranges[k];
      memcpy(&out_frame[i], &r->lower_freq, 4); i += 4;
      memcpy(&out_frame[i], &r->upper_freq, 4); i += 4;
    }
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_RAW_PACKET && len >= 4) {
    auto pkt = obtainNewPacket();
    if (pkt) {
      uint8_t priority = cmd_frame[1];
      if (tryParsePacket(pkt, &cmd_frame[2], len - 2)) {
        sendPacket(pkt, priority, 0);
        writeOKFrame();
      } else {
        releasePacket(pkt);
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    MESH_DEBUG_PRINTLN("ERROR: unknown command: %02X", cmd_frame[0]);
  }
}

static bool save_filter(const ContactInfo& c) {
  return c.type != ADV_TYPE_NONE;   // don't save the transient/anon entries
}

void MyMesh::saveContacts() {
  _store->saveContacts(this, save_filter);
}

void MyMesh::enterCLIRescue() {
  _cli_rescue = true;
  cli_command[0] = 0;
  Serial.println("========= CLI Rescue =========");
}

void MyMesh::checkCLIRescueCmd() {
  int len = strlen(cli_command);
  while (Serial.available() && len < sizeof(cli_command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      cli_command[len++] = c;
      cli_command[len] = 0;
    }
    Serial.print(c);  // echo
  }
  if (len == sizeof(cli_command)-1) {  // command buffer full
    cli_command[sizeof(cli_command)-1] = '\r';
  }

  if (len > 0 && cli_command[len - 1] == '\r') {  // received complete line
    cli_command[len - 1] = 0;  // replace newline with C string null terminator

    if (memcmp(cli_command, "set ", 4) == 0) {
      const char* config = &cli_command[4];
      if (memcmp(config, "pin ", 4) == 0) {
        _prefs.ble_pin = atoi(&config[4]);
        dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
        Serial.printf("  > pin is now %06d\n", _prefs.ble_pin);
      } else {
        Serial.printf("  Error: unknown config: %s\n", config);
      }
    } else if (strcmp(cli_command, "rebuild") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        _store->saveMainIdentity(self_id);
        savePrefs();
        saveContacts();
        saveChannels();
        Serial.println("  > erase and rebuild done");
      } else {
        Serial.println("  Error: erase failed");
      }
    } else if (strcmp(cli_command, "erase") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        Serial.println("  > erase done");
      } else {
        Serial.println("  Error: erase failed");
      }
    } else if (memcmp(cli_command, "ls", 2) == 0) {

      // get path from command e.g: "ls /adafruit"
      const char *path = &cli_command[3];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }
      Serial.printf("Listing files in %s\n", path);

      // log each file and directory
      File root = _store->openRead(path);
      if (is_fs2 == false) {
        if (root) {
          File file = root.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  UserData%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] UserData%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root.openNextFile();
          }
          root.close();
        }
      }

      if (is_fs2 == true || strlen(path) == 0 || strcmp(path, "/") == 0) {
        if (_store->getSecondaryFS() != nullptr) {
          File root2 = _store->openRead(_store->getSecondaryFS(), path);
          File file = root2.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  ExtraFS%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] ExtraFS%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root2.openNextFile();
          }
          root2.close();
        }
      }
    } else if (memcmp(cli_command, "cat", 3) == 0) {

      // get path from command e.g: "cat /contacts3"
      const char *path = &cli_command[4];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      } else {
        Serial.println("Invalid path provided, must start with UserData/ or ExtraFS/");
        cli_command[0] = 0;
        return;
      }

      // log file content as hex
      File file = _store->openRead(path);
      if (is_fs2 == true) {
        file = _store->openRead(_store->getSecondaryFS(), path);
      }
      if(file){

        // get file content
        int file_size = file.available();
        uint8_t buffer[file_size];
        file.read(buffer, file_size);

        // print hex
        mesh::Utils::printHex(Serial, buffer, file_size);
        Serial.print("\n");

        file.close();

      }

    } else if (memcmp(cli_command, "rm ", 3) == 0) {
      // get path from command e.g: "rm /adv_blobs"
      const char *path = &cli_command[3];
      MESH_DEBUG_PRINTLN("Removing file: %s", path);
      // ensure path is not empty, or root dir
      if(!path || strlen(path) == 0 || strcmp(path, "/") == 0){
        Serial.println("Invalid path provided");
      } else {
      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }

        // remove file
        bool removed;
        if (is_fs2) {
          MESH_DEBUG_PRINTLN("Removing file from ExtraFS: %s", path);
          removed = _store->removeFile(_store->getSecondaryFS(), path);
        } else {
          MESH_DEBUG_PRINTLN("Removing file from UserData: %s", path);
          removed = _store->removeFile(path);
        }
        if(removed){
          Serial.println("File removed");
        } else {
          Serial.println("Failed to remove file");
        }

      }

    } else if (strcmp(cli_command, "reboot") == 0) {
      board.reboot();  // doesn't return
    } else if (memcmp(cli_command, "timesync", 8) == 0) {
      char reply[320];
      _ts.buildReply(reply, getRTCClock());
      Serial.println(reply);
    } else {
      Serial.println("  Error: unknown command");
    }

    cli_command[0] = 0;  // reset command buffer
  }
}

void MyMesh::checkSerialInterface() {
  size_t len = _serial->checkRecvFrame(cmd_frame);
  if (len > 0) {
    handleCmdFrame(len);
  } else if (_iter_started              // check if our ContactsIterator is 'running'
             && !_serial->isWriteBusy() // don't spam the Serial Interface too quickly!
  ) {
    ContactInfo contact;
    if (_iter.hasNext(this, contact)) {
      if (contact.lastmod > _iter_filter_since) { // apply the 'since' filter
        writeContactRespFrame(RESP_CODE_CONTACT, contact);
        if (contact.lastmod > _most_recent_lastmod) {
          _most_recent_lastmod = contact.lastmod; // save for the RESP_CODE_END_OF_CONTACTS frame
        }
      }
    } else { // EOF
      out_frame[0] = RESP_CODE_END_OF_CONTACTS;
      memcpy(&out_frame[1], &_most_recent_lastmod,
             4); // include the most recent lastmod, so app can update their 'since'
      _serial->writeFrame(out_frame, 5);
      _iter_started = false;
    }
  //} else if (!_serial->isWriteBusy()) {
  //  checkConnections();    // TODO - deprecate the 'Connections' stuff
  }
}

void MyMesh::loop() {
  BaseChatMesh::loop();

  if (_pending_reboot_at && millisHasNowPassed(_pending_reboot_at)) {
#ifdef NRF52_PLATFORM
    bool ble_busy = _serial && _serial->isConnected();
#else
    bool ble_busy = false;
#endif
    bool has_dirty = dirty_contacts_expiry || dirty_prefs_expiry;
    if (ble_busy && has_dirty && !millisHasNowPassed(_pending_reboot_deadline)) {
      _serial->disconnect();  // NRF52: kick BLE so flash is available for save
      _pending_reboot_at = futureMillis(500);  // wait for disconnect to complete
    } else {
      _pending_reboot_at = 0;
      if (!ble_busy) {
        if (dirty_contacts_expiry) { saveContacts(); dirty_contacts_expiry = 0; }
        if (dirty_prefs_expiry)    { savePrefs();    dirty_prefs_expiry    = 0; }
      }
#ifdef ESP32
      SPIFFS.end();
#endif
      board.reboot();
    }
  }
  if (_pending_poweroff_at && millisHasNowPassed(_pending_poweroff_at)) {
#ifdef NRF52_PLATFORM
    bool ble_busy = _serial && _serial->isConnected();
#else
    bool ble_busy = false;
#endif
    bool has_dirty = dirty_contacts_expiry || dirty_prefs_expiry;
    if (ble_busy && has_dirty && !millisHasNowPassed(_pending_poweroff_deadline)) {
      _serial->disconnect();  // NRF52: kick BLE so flash is available for save
      _pending_poweroff_at = futureMillis(500);  // wait for disconnect to complete
    } else {
      _pending_poweroff_at = 0;
      if (!ble_busy) {
        if (dirty_contacts_expiry) { saveContacts(); dirty_contacts_expiry = 0; }
        if (dirty_prefs_expiry)    { savePrefs();    dirty_prefs_expiry    = 0; }
      }
#ifdef ESP32
      SPIFFS.end();
#endif
      board.powerOff();
    }
  }

#ifdef WITH_WIFI_SWITCHING
  checkWifiConnection();
#endif

  if (_cli_rescue) {
    checkCLIRescueCmd();
  } else if (_serial) {
    checkSerialInterface();
  }

  // On NRF52 with SoftDevice, flash writes block if BLE is active (S140 v6.1.1 known issue).
  // Defer until after disconnect; retry every 1 s while connected.
#ifdef NRF52_PLATFORM
  bool ble_busy = _serial && _serial->isConnected();
#else
  bool ble_busy = false;
#endif
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    if (!ble_busy) {
      saveContacts();
      dirty_contacts_expiry = 0;
    } else {
      dirty_contacts_expiry = futureMillis(1000);
    }
  }
  if (dirty_prefs_expiry && millisHasNowPassed(dirty_prefs_expiry)) {
    if (!ble_busy) {
      savePrefs();
      dirty_prefs_expiry = 0;
    } else {
      dirty_prefs_expiry = futureMillis(1000);
    }
  }

#ifdef DISPLAY_CLASS
  if (_ui && _serial) _ui->setHasConnection(_serial->isConnected());
#endif
}

bool MyMesh::advert() {
  mesh::Packet* pkt;
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
    pkt = createSelfAdvert(_prefs.node_name);
  } else {
    pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
  }
  if (pkt) {
    sendZeroHop(pkt);
    return true;
  } else {
    return false;
  }
}

#ifdef WITH_COMPANION_CLI

// Split buf into chunks of <=150 chars, trying to break on newlines.
// Returns number of chunks written into out[].
static int splitCliReply(const char* buf, char out[][152], int max_chunks) {
  int n = 0;
  const char* p = buf;
  while (*p && n < max_chunks) {
    int remaining = strlen(p);
    if (remaining <= 150) {
      strcpy(out[n++], p);
      break;
    }
    // Find last newline within 150 chars
    int cut = 150;
    for (int i = 149; i > 0; i--) {
      if (p[i] == '\n') { cut = i + 1; break; }
    }
    memcpy(out[n], p, cut);
    out[n][cut] = '\0';
    n++;
    p += cut;
  }
  return n;
}

void MyMesh::sendCliReplyPM(const ContactInfo& to, const char* buf) {
  // static: loop_task is single-threaded; these are never called re-entrantly.
  // Without static, chunks[8][152]=1216B + handleRemoteCLI's buf[512]+cmdBuf[256]
  // overflows the 4096B FreeRTOS loop_task stack → immediate hard fault, no log output.
  static char chunks[8][152];
  static char text[160];
  int n = splitCliReply(buf, chunks, 8);
  uint32_t ack_dummy, timeout_dummy;
  for (int i = 0; i < n; i++) {
    if (n > 1)
      snprintf(text, sizeof(text), "[%d/%d] %s", i + 1, n, chunks[i]);
    else
      strncpy(text, chunks[i], sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    sendMessage(to, getRTCClock()->getCurrentTimeUnique(), 0, text, ack_dummy, timeout_dummy);
  }
}

void MyMesh::sendCliReplyChannel(uint8_t ch_idx, const char* buf, bool mirror_ui) {
  static char chunks[8][152];
  static char text[200];
  int n = splitCliReply(buf, chunks, 8);
  uint32_t now = getRTCClock()->getCurrentTimeUnique();
  const char* channel_name = "TerminalCLI";
#ifdef DISPLAY_CLASS
  if (mirror_ui) {
    ChannelDetails ch;
    if (getChannel(ch_idx, ch) && ch.name[0]) channel_name = ch.name;
  }
#endif

  for (int i = 0; i < n; i++) {
    if (n > 1)
      snprintf(text, sizeof(text), "%s: [%d/%d] %s", _prefs.node_name, i + 1, n, chunks[i]);
    else
      snprintf(text, sizeof(text), "%s: %s", _prefs.node_name, chunks[i]);

    int fi = 0;
    if (app_target_ver >= 3) {
      out_frame[fi++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
      out_frame[fi++] = 0;  // SNR (synthetic)
      out_frame[fi++] = 0;  // reserved
      out_frame[fi++] = 0;  // reserved
    } else {
      out_frame[fi++] = RESP_CODE_CHANNEL_MSG_RECV;
    }
    out_frame[fi++] = ch_idx;
    out_frame[fi++] = 0; // synthetic local reply, 0 LoRa hops
    out_frame[fi++] = TXT_TYPE_PLAIN;
    memcpy(&out_frame[fi], &now, 4); fi += 4;
    int tlen = strlen(text);
    if (fi + tlen > MAX_FRAME_SIZE) tlen = MAX_FRAME_SIZE - fi;
    memcpy(&out_frame[fi], text, tlen); fi += tlen;
    addToOfflineQueue(out_frame, fi);
#ifdef DISPLAY_CLASS
    if (mirror_ui && _ui) _ui->newMsg(0, channel_name, text, offline_queue_len);
#endif
    now++;  // unique timestamp per chunk
  }

  if (_serial->isConnected()) {
    uint8_t frame[1] = { PUSH_CODE_MSG_WAITING };
    _serial->writeFrame(frame, 1);
  }
}

#endif

bool MyMesh::sendGroupMessageWithCyr2LatMap(uint32_t timestamp, mesh::GroupChannel& channel, const char* sender_name,
                                            const char* text, int text_len, const char* original_text,
                                            int original_len, bool record_map) {
  uint8_t temp[5 + MAX_TEXT_LEN + 32];
  memcpy(temp, &timestamp, 4);
  temp[4] = 0;  // TXT_TYPE_PLAIN

  sprintf((char *)&temp[5], "%s: ", sender_name);
  char* ep = strchr((char *)&temp[5], 0);
  int prefix_len = ep - (char *)&temp[5];

  if (text_len + prefix_len > MAX_TEXT_LEN) text_len = MAX_TEXT_LEN - prefix_len;
  memcpy(ep, text, text_len);
  ep[text_len] = 0;

  auto pkt = createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, channel, temp, 5 + prefix_len + text_len);
  if (!pkt) return false;

  if (record_map) {
    Cyr2LatChannelMap& map = _cyr2lat_channel_maps[_next_cyr2lat_channel_map];
    _next_cyr2lat_channel_map = (_next_cyr2lat_channel_map + 1) % CYR2LAT_CHANNEL_MAP_SIZE;

    pkt->calculatePacketHash(map.transformed_hash);

    uint8_t original[5 + MAX_TEXT_LEN + 32];
    memcpy(original, &timestamp, 4);
    original[4] = 0;
    sprintf((char *)&original[5], "%s: ", sender_name);
    char* original_ep = strchr((char *)&original[5], 0);
    int original_prefix_len = original_ep - (char *)&original[5];
    if (original_len + original_prefix_len > MAX_TEXT_LEN) {
      original_len = MAX_TEXT_LEN - original_prefix_len;
      if (original_len < 0) original_len = 0;
    }
    memcpy(original_ep, original_text, original_len);
    original_ep[original_len] = 0;

    int payload_len = 0;
    memcpy(&map.original_payload[payload_len], channel.hash, PATH_HASH_SIZE);
    payload_len += PATH_HASH_SIZE;
    payload_len += mesh::Utils::encryptThenMAC(channel.secret, &map.original_payload[payload_len],
                                               original, 5 + original_prefix_len + original_len);
    map.original_payload_len = payload_len;
  }

  TransportKey default_scope;
  memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));
  auto scope = send_scope.isNull() ? &default_scope : &send_scope;

  pkt->header &= ~PH_ROUTE_MASK;
  if (scope->isNull()) {
    pkt->header |= ROUTE_TYPE_FLOOD;
  } else {
    pkt->header |= ROUTE_TYPE_TRANSPORT_FLOOD;
    pkt->transport_codes[0] = scope->calcTransportCode(pkt);
    pkt->transport_codes[1] = 0;
  }
  pkt->setPathHashSizeAndCount(_prefs.path_hash_mode + 1, 0);

  getTables()->markSeen(pkt);  // mark own packet as seen so a repeater echo is dropped at dedupe
  sendPacket(pkt, 1);
  return true;
}

int MyMesh::mapCyr2LatChannelRawLog(const uint8_t* raw, int len, uint8_t* mapped, int mapped_size) {
  if (len < 2) return 0;

  mesh::Packet pkt;
  if (!pkt.readFrom(raw, len) || pkt.getPayloadType() != PAYLOAD_TYPE_GRP_TXT) return 0;

  uint8_t hash[MAX_HASH_SIZE];
  pkt.calculatePacketHash(hash);
  for (int i = 0; i < CYR2LAT_CHANNEL_MAP_SIZE; i++) {
    Cyr2LatChannelMap& map = _cyr2lat_channel_maps[i];
    if (map.original_payload_len == 0 || memcmp(hash, map.transformed_hash, MAX_HASH_SIZE) != 0) continue;

    pkt.payload_len = map.original_payload_len;
    memcpy(pkt.payload, map.original_payload, map.original_payload_len);
    if (pkt.getRawLength() > mapped_size) return 0;
    return pkt.writeTo(mapped);
  }
  return 0;
}

#ifdef WITH_COMPANION_CLI

bool MyMesh::handleCliCmd(uint32_t sender_ts, const char* cmd, char* buf, bool is_remote) {
  if (strcmp(cmd, "pin") == 0) {
    if (_prefs.ble_pin == 0)
      snprintf(buf, 512, "CLI PIN: %s  BLE PIN: %06lu (auto, new random on reboot)", _cli_pin, (unsigned long)_active_ble_pin);
    else
      snprintf(buf, 512, "CLI PIN: %s  BLE PIN: %06lu", _cli_pin, (unsigned long)_prefs.ble_pin);

  } else if (strcmp(cmd, "get ble.pin") == 0) {
    if (_prefs.ble_pin == 0)
      snprintf(buf, 512, "ble.pin: auto (current session: %06lu)", (unsigned long)_active_ble_pin);
    else
      snprintf(buf, 512, "ble.pin: %06lu", (unsigned long)_prefs.ble_pin);
  } else if (strncmp(cmd, "set ble.pin ", 12) == 0) {
    uint32_t pin = (uint32_t)atol(cmd + 12);
    if (pin == 0 || (pin >= 100000 && pin <= 999999)) {
      _prefs.ble_pin = pin;
      if (pin != 0) _active_ble_pin = pin;  // don't zero out display — pin unchanged until reboot
      dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
      if (pin == 0)
        strcpy(buf, "ble.pin: auto (new random on reboot)");
      else
        snprintf(buf, 512, "ble.pin: %06lu (reboot to apply)", (unsigned long)pin);
    } else {
      strcpy(buf, "ERR: pin must be 0 (auto/random) or 100000-999999");
    }

  } else if (strcmp(cmd, "timesync") == 0) {
    _ts.buildReply(buf, getRTCClock());
    char flags[64];
    snprintf(flags, sizeof(flags), "\nAdverts: %s  Msgs: %s",
             _ts_from_adverts ? "on" : "off", _ts_from_messages ? "on" : "off");
    strncat(buf, flags, 511 - strlen(buf));

  } else if (strcmp(cmd, "timesync on") == 0) {
    _ts_from_adverts = _ts_from_messages = true;
    strcpy(buf, "timesync: on (adverts+msgs)");
  } else if (strcmp(cmd, "timesync off") == 0) {
    _ts_from_adverts = _ts_from_messages = false;
    strcpy(buf, "timesync: off");
  } else if (strcmp(cmd, "timesync adverts on") == 0) {
    _ts_from_adverts = true;
    strcpy(buf, "timesync adverts: on");
  } else if (strcmp(cmd, "timesync adverts off") == 0) {
    _ts_from_adverts = false;
    strcpy(buf, "timesync adverts: off");
  } else if (strcmp(cmd, "timesync msgs on") == 0) {
    _ts_from_messages = true;
    strcpy(buf, "timesync msgs: on");
  } else if (strcmp(cmd, "timesync msgs off") == 0) {
    _ts_from_messages = false;
    strcpy(buf, "timesync msgs: off");
  } else if (strcmp(cmd, "timesync reset") == 0) {
    _ts.reset();
    strcpy(buf, "timesync: state reset (params kept)");

  } else if (strcmp(cmd, "timesync params") == 0) {
    snprintf(buf, 512, "cluster: %lus (default %us)\ndrift:   %lus (default %us)\njump:    %lus (default %us)",
             (unsigned long)_ts.cluster_window,  TS_CLUSTER_WINDOW,
             (unsigned long)_ts.drift_threshold, TS_DRIFT_THRESHOLD,
             (unsigned long)_ts.max_jump,         TS_MAX_JUMP);
  } else if (strncmp(cmd, "set timesync.cluster ", 21) == 0) {
    _ts.cluster_window = atoi(cmd + 21);
    snprintf(buf, 512, "cluster_window: %lus", (unsigned long)_ts.cluster_window);
  } else if (strncmp(cmd, "set timesync.drift ", 19) == 0) {
    _ts.drift_threshold = atoi(cmd + 19);
    snprintf(buf, 512, "drift_threshold: %lus", (unsigned long)_ts.drift_threshold);
  } else if (strncmp(cmd, "set timesync.jump ", 18) == 0) {
    _ts.max_jump = atoi(cmd + 18);
    snprintf(buf, 512, "max_jump: %lus", (unsigned long)_ts.max_jump);

  } else if (strcmp(cmd, "get remote.cli") == 0) {
    snprintf(buf, 512, "remote.cli: %s", _remote_cli_enabled ? "on" : "off");
  } else if (strcmp(cmd, "set remote.cli on") == 0) {
    _remote_cli_enabled = true;
    strcpy(buf, "remote.cli: on");
  } else if (strcmp(cmd, "set remote.cli off") == 0) {
    _remote_cli_enabled = false;
    if (is_remote) strcpy(buf, "remote.cli: off (this was the last remote command)");
    else           strcpy(buf, "remote.cli: off");

  } else if (strcmp(cmd, "get terminal.cli") == 0) {
    snprintf(buf, 512, "terminal.cli: %s", _terminal_cli_enabled ? "on" : "off");
  } else if (strcmp(cmd, "set terminal.cli on") == 0) {
    if (!_terminal_cli_enabled) {
      if (addChannel("TerminalCLI", TERMINAL_CLI_PSK)) {
        saveChannels();
        _terminal_cli_enabled = true;
        strcpy(buf, "terminal.cli: on (channel registered)");
      } else {
        strcpy(buf, "ERR: addChannel failed (slots full?)");
      }
    } else {
      strcpy(buf, "terminal.cli: already on");
    }
  } else if (strcmp(cmd, "set terminal.cli off") == 0) {
    bool found = false;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails ch;
      if (getChannel(i, ch) && ch.name[0] && strcmp(ch.name, "TerminalCLI") == 0) {
        ChannelDetails empty{};
        setChannel(i, empty);
        found = true;
        break;
      }
    }
    if (found) saveChannels();
    _terminal_cli_enabled = false;
    strcpy(buf, found ? "terminal.cli: off (channel removed)" : "terminal.cli: off");
  } else if (strcmp(cmd, "get cyr2lat.channels") == 0) {
    snprintf(buf, 512, "cyr2lat.channels: %s", _cyr2lat_channels_enabled ? "on" : "off");
  } else if (strcmp(cmd, "set cyr2lat.channels on") == 0) {
    _cyr2lat_channels_enabled = true;
    _prefs.cyr2lat_channels = 1;
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
    strcpy(buf, "cyr2lat.channels: on");
  } else if (strcmp(cmd, "set cyr2lat.channels off") == 0) {
    _cyr2lat_channels_enabled = false;
    _prefs.cyr2lat_channels = 0;
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
    strcpy(buf, "cyr2lat.channels: off");
  } else if (strcmp(cmd, "get cyr2lat.contacts") == 0) {
    snprintf(buf, 512, "cyr2lat.contacts: %s", _cyr2lat_contacts_enabled ? "on" : "off");
  } else if (strcmp(cmd, "set cyr2lat.contacts on") == 0) {
    _cyr2lat_contacts_enabled = true;
    _prefs.cyr2lat_contacts = 1;
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
    strcpy(buf, "cyr2lat.contacts: on");
  } else if (strcmp(cmd, "set cyr2lat.contacts off") == 0) {
    _cyr2lat_contacts_enabled = false;
    _prefs.cyr2lat_contacts = 0;
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
    strcpy(buf, "cyr2lat.contacts: off");

#ifdef WITH_WIFI_SWITCHING
  } else if (strcmp(cmd, "wifi list") == 0) {
    if (_wifi_prefs.network_count == 0) {
      strcpy(buf, "No WiFi networks stored. Use: wifi add <ssid> <pass>");
    } else {
      int n = 0;
      for (int i = 0; i < _wifi_prefs.network_count; i++)
        n += snprintf(buf + n, 512 - n, "[%d] %s\n", i, _wifi_prefs.networks[i].ssid);
      snprintf(buf + n, 512 - n, "mode: %s  port: %u",
               _wifi_prefs.ip_mode == IP_MODE_STATIC ? "static" : "dhcp",
               (unsigned)(_wifi_prefs.tcp_port ? _wifi_prefs.tcp_port : WIFI_TCP_PORT_DEFAULT));
    }

  } else if (strcmp(cmd, "wifi scan") == 0) {
    strcpy(buf, "Scanning...");
    int n_found = WiFi.scanNetworks();
    if (n_found <= 0) {
      strcpy(buf, "No networks found.");
    } else {
      int n = 0;
      for (int i = 0; i < n_found && n < 500; i++)
        n += snprintf(buf + n, 512 - n, "%s (%d dBm)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      WiFi.scanDelete();
    }

  } else if (strncmp(cmd, "wifi add ", 9) == 0) {
    char ssid[33] = {}, pass[65] = {};
    const char* p = cmd + 9;
    // Parse SSID: quoted ("My Network") or plain (NoSpaces)
    if (*p == '"') {
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 32) ssid[i++] = *p++;
      if (*p == '"') p++;
    } else {
      int i = 0;
      while (*p && *p != ' ' && i < 32) ssid[i++] = *p++;
    }
    while (*p == ' ') p++;
    // Parse password: quoted or plain (rest of string)
    if (*p == '"') {
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 64) pass[i++] = *p++;
    } else {
      strncpy(pass, p, 64);
    }
    if (ssid[0]) {
      if (_wifi_prefs.network_count >= WIFI_MAX_NETWORKS) {
        strcpy(buf, "ERR: max 5 networks stored");
      } else {
        addWifiNetwork(ssid, pass);
        snprintf(buf, 512, "wifi: saved [%s]", ssid);
      }
    } else {
      strcpy(buf, "Usage: wifi add <ssid> [password]\n  Quotes for spaces: wifi add \"My Network\" \"my pass\"");
    }

  } else if (strncmp(cmd, "wifi del ", 9) == 0) {
    const char* p = cmd + 9;
    char ssid[33] = {};
    if (*p == '"') {
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 32) ssid[i++] = *p++;
    } else {
      strncpy(ssid, p, 32);
    }
    removeWifiNetwork(ssid);
    snprintf(buf, 512, "wifi: removed [%s] (if existed)", ssid);

  } else if (strcmp(cmd, "wifi ip dhcp") == 0) {
    _wifi_prefs.ip_mode = IP_MODE_DHCP;
    saveWifiPrefs();
    strcpy(buf, "wifi ip: dhcp");

  } else if (strncmp(cmd, "wifi ip static ", 15) == 0) {
    char ip[16] = {}, gw[16] = {}, mask[16] = {};
    if (sscanf(cmd + 15, "%15s %15s %15s", ip, gw, mask) == 3) {
      _wifi_prefs.ip_mode = IP_MODE_STATIC;
      strncpy(_wifi_prefs.static_ip,   ip,   sizeof(_wifi_prefs.static_ip) - 1);
      strncpy(_wifi_prefs.static_gw,   gw,   sizeof(_wifi_prefs.static_gw) - 1);
      strncpy(_wifi_prefs.static_mask, mask, sizeof(_wifi_prefs.static_mask) - 1);
      saveWifiPrefs();
      snprintf(buf, 512, "wifi ip: static %s gw %s mask %s", ip, gw, mask);
    } else {
      strcpy(buf, "Usage: wifi ip static <ip> <gateway> <mask>");
    }

  } else if (strncmp(cmd, "wifi port ", 10) == 0) {
    int port = atoi(cmd + 10);
    if (port > 0 && port < 65536) {
      _wifi_prefs.tcp_port = (uint16_t)port;
      saveWifiPrefs();
      snprintf(buf, 512, "wifi port: %d", port);
    } else {
      strcpy(buf, "Usage: wifi port <1-65535>");
    }

  } else if (strncmp(cmd, "wifi connect ", 13) == 0) {
    int idx = atoi(cmd + 13);
    if (idx >= 0 && idx < _wifi_prefs.network_count) {
      snprintf(buf, 512, "connecting to [%s]...", _wifi_prefs.networks[idx].ssid);
      switchCommsMode(COMMS_MODE_WIFI, idx);
    } else {
      snprintf(buf, 512, "ERR: index %d out of range (0..%d)", idx, _wifi_prefs.network_count - 1);
    }

  } else if (strcmp(cmd, "wifi status") == 0) {
    const char* mode_str = (_wifi_prefs.comms_mode == COMMS_MODE_WIFI) ? "WiFi"
                         : (_wifi_prefs.comms_mode == COMMS_MODE_USB)  ? "USB" : "BLE";
    if (_wifi_connecting) {
      snprintf(buf, 512, "comms: %s (connecting...)", mode_str);
    } else if (_wifi_prefs.comms_mode == COMMS_MODE_WIFI && WiFi.status() == WL_CONNECTED) {
      snprintf(buf, 512, "comms: WiFi  ip: %s  port: %u",
               WiFi.localIP().toString().c_str(),
               (unsigned)(_wifi_prefs.tcp_port ? _wifi_prefs.tcp_port : WIFI_TCP_PORT_DEFAULT));
    } else {
      snprintf(buf, 512, "comms: %s", mode_str);
    }
#endif  // WITH_WIFI_SWITCHING

  } else {
    return false; // not a companion command — delegate to CommonCLI
  }
  return true;
}

void MyMesh::handleRemoteCLI(const ContactInfo& from, uint32_t sender_ts, const char* cmd) {
  char from_hex[9];
  mesh::Utils::toHex(from_hex, from.id.pub_key, 4);

  // static: saves 768B (cmdBuf+buf) from the loop_task stack on every call
  static char cmdBuf[256];
  strncpy(cmdBuf, cmd, sizeof(cmdBuf) - 1);
  cmdBuf[sizeof(cmdBuf) - 1] = '\0';
  char* e = cmdBuf + strlen(cmdBuf);
  while (e > cmdBuf && (e[-1] == ' ' || e[-1] == '\r' || e[-1] == '\n')) *--e = '\0';
  cmd = cmdBuf;

  MESH_DEBUG_PRINTLN("CLI/PM from=%s cmd='%s'", from_hex, cmd);

  static char buf[512];
  buf[0] = '\0';
  if (strcmp(cmd, "reboot") == 0) {
    sendCliReplyPM(from, "rebooting in 1s...");
    _pending_reboot_at = futureMillis(1000);
    _pending_reboot_deadline = futureMillis(30000);
    return;
  } else if (strcmp(cmd, "poweroff") == 0 || strcmp(cmd, "shutdown") == 0) {
    sendCliReplyPM(from, "powering off...");
    _pending_poweroff_at = futureMillis(500);
    _pending_poweroff_deadline = futureMillis(30000);
    return;
  } else if (!handleCliCmd(sender_ts, cmd, buf, true)) {
    if (_cli) _cli->handleCommand(sender_ts, const_cast<char*>(cmd), buf);
    else      strcpy(buf, "ERR: CLI not initialized");
  }
  MESH_DEBUG_PRINTLN("CLI/PM reply(%zu): '%s'", strlen(buf), buf);
  if (buf[0]) sendCliReplyPM(from, buf);
}

void MyMesh::handleTerminalCLI(uint8_t ch_idx, uint32_t sender_ts, const char* cmd,
                               bool write_ack, bool mirror_ui_reply) {
  static char cmdBuf[256];
  strncpy(cmdBuf, cmd, sizeof(cmdBuf) - 1);
  cmdBuf[sizeof(cmdBuf) - 1] = '\0';
  char* e = cmdBuf + strlen(cmdBuf);
  while (e > cmdBuf && (e[-1] == ' ' || e[-1] == '\r' || e[-1] == '\n')) *--e = '\0';
  cmd = cmdBuf;

  MESH_DEBUG_PRINTLN("CLI/Terminal ch=%u cmd='%s'", ch_idx, cmd);

  static char buf[512];
  buf[0] = '\0';
  if (strcmp(cmd, "reboot") == 0) {
    strcpy(buf, "rebooting in 1s...");
    _pending_reboot_at = futureMillis(1000);
    _pending_reboot_deadline = futureMillis(30000);
  } else if (strcmp(cmd, "poweroff") == 0 || strcmp(cmd, "shutdown") == 0) {
    strcpy(buf, "powering off...");
    _pending_poweroff_at = futureMillis(500);
    _pending_poweroff_deadline = futureMillis(30000);
  } else if (!handleCliCmd(sender_ts, cmd, buf, false)) {
    if (_cli) _cli->handleCommand(sender_ts, const_cast<char*>(cmd), buf);
    else      strcpy(buf, "ERR: CLI not initialized");
  }
  MESH_DEBUG_PRINTLN("CLI/Terminal reply(%zu): '%s'", strlen(buf), buf);
  if (write_ack) writeOKFrame(); // send OK before push so app completes the command exchange first
  if (buf[0]) sendCliReplyChannel(ch_idx, buf, mirror_ui_reply);
}

#endif

void MyMesh::setCyr2LatChannelsEnabled(bool enabled) {
  _cyr2lat_channels_enabled = enabled;
  _prefs.cyr2lat_channels = enabled ? 1 : 0;
  dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
}

void MyMesh::setCyr2LatContactsEnabled(bool enabled) {
  _cyr2lat_contacts_enabled = enabled;
  _prefs.cyr2lat_contacts = enabled ? 1 : 0;
  dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
}

#ifdef WITH_WIFI_SWITCHING

void MyMesh::loadWifiPrefs() {
  memset(&_wifi_prefs, 0, sizeof(_wifi_prefs));
  _wifi_prefs.tcp_port = WIFI_TCP_PORT_DEFAULT;
  File file = SPIFFS.open("/wifi_prefs", "r");
  if (file) {
    file.read((uint8_t*)&_wifi_prefs, min((size_t)file.size(), sizeof(_wifi_prefs)));
    file.close();
  }
  if (_wifi_prefs.tcp_port == 0) _wifi_prefs.tcp_port = WIFI_TCP_PORT_DEFAULT;
}

void MyMesh::saveWifiPrefs() {
  File file = SPIFFS.open("/wifi_prefs", "w");
  if (file) {
    file.write((uint8_t*)&_wifi_prefs, sizeof(_wifi_prefs));
    file.close();
  }
}

void MyMesh::addWifiNetwork(const char* ssid, const char* pass) {
  // Update existing entry if SSID already stored
  for (int i = 0; i < _wifi_prefs.network_count; i++) {
    if (strcmp(_wifi_prefs.networks[i].ssid, ssid) == 0) {
      strncpy(_wifi_prefs.networks[i].password, pass, sizeof(_wifi_prefs.networks[i].password) - 1);
      saveWifiPrefs();
      return;
    }
  }
  if (_wifi_prefs.network_count >= WIFI_MAX_NETWORKS) return;  // full
  int idx = _wifi_prefs.network_count++;
  strncpy(_wifi_prefs.networks[idx].ssid,     ssid, sizeof(_wifi_prefs.networks[idx].ssid) - 1);
  strncpy(_wifi_prefs.networks[idx].password, pass, sizeof(_wifi_prefs.networks[idx].password) - 1);
  saveWifiPrefs();
}

void MyMesh::removeWifiNetwork(const char* ssid) {
  for (int i = 0; i < _wifi_prefs.network_count; i++) {
    if (strcmp(_wifi_prefs.networks[i].ssid, ssid) == 0) {
      // Shift remaining entries left
      for (int j = i; j < _wifi_prefs.network_count - 1; j++)
        _wifi_prefs.networks[j] = _wifi_prefs.networks[j + 1];
      memset(&_wifi_prefs.networks[--_wifi_prefs.network_count], 0, sizeof(WifiNetwork));
      saveWifiPrefs();
      return;
    }
  }
}

void MyMesh::switchCommsMode(uint8_t mode, int wifi_net_idx) {
  if (_serial) _serial->disable();

  _wifi_prefs.comms_mode = mode;
  saveWifiPrefs();

  if (mode == COMMS_MODE_WIFI && wifi_net_idx >= 0
      && wifi_net_idx < _wifi_prefs.network_count) {
    _wifi_net_idx = wifi_net_idx;
    _wifi_connecting = true;
    _wifi_connect_start = millis();
    WiFi.begin(_wifi_prefs.networks[wifi_net_idx].ssid,
               _wifi_prefs.networks[wifi_net_idx].password);
    // Keep BLE active as fallback during connection attempt
    _serial = &_ble_iface;
    _serial->enable();
  } else if (mode == COMMS_MODE_USB) {
    WiFi.disconnect(true);
    _serial = nullptr;  // USB is managed externally
  } else {
    // BLE — explicit user switch: cancel background retry, shut down WiFi
    _wifi_bg_retry_at = 0;
    WiFi.disconnect(true);
    _wifi_prefs.comms_mode = COMMS_MODE_BLE;
    saveWifiPrefs();
    _serial = &_ble_iface;
    _serial->enable();
  }
}

void MyMesh::checkWifiConnection() {
  // Background retry: reconnect to WiFi every 30 sec after fallback to BLE
  if (!_wifi_connecting && _wifi_bg_retry_at && millis() >= _wifi_bg_retry_at) {
    _wifi_bg_retry_at = 0;
    if (_wifi_net_idx >= 0 && _wifi_net_idx < _wifi_prefs.network_count) {
      _wifi_connecting = true;
      _wifi_connect_start = millis();
      WiFi.begin(_wifi_prefs.networks[_wifi_net_idx].ssid,
                 _wifi_prefs.networks[_wifi_net_idx].password);
    }
  }

  if (!_wifi_connecting) return;

  if (WiFi.status() == WL_CONNECTED) {
    _wifi_connecting = false;
    if (_wifi_prefs.ip_mode == IP_MODE_STATIC) {
      IPAddress ip, gw, mask;
      ip.fromString(_wifi_prefs.static_ip);
      gw.fromString(_wifi_prefs.static_gw);
      mask.fromString(_wifi_prefs.static_mask);
      WiFi.config(ip, gw, mask);
    }
    board.setInhibitSleep(true);
    uint16_t port = _wifi_prefs.tcp_port ? _wifi_prefs.tcp_port : WIFI_TCP_PORT_DEFAULT;
    if (_serial) _serial->disable();
    _wifi_iface.begin(port);
    _serial = &_wifi_iface;
    _serial->enable();
    if (_wifi_prefs.comms_mode != COMMS_MODE_WIFI) {
      _wifi_prefs.comms_mode = COMMS_MODE_WIFI;
      saveWifiPrefs();
    }
  } else if (millis() - _wifi_connect_start > 15000) {
    _wifi_connecting = false;
    _wifi_prefs.comms_mode = COMMS_MODE_BLE;
    saveWifiPrefs();
    WiFi.disconnect(false);  // keep radio on for background retry
    _wifi_bg_retry_at = millis() + 30000;
    // _serial already points to _ble_iface (set in switchCommsMode)
  }
}

void MyMesh::initCommsFromPrefs() {
  loadWifiPrefs();
  if (_active_ble_pin == 0) {
    if (_prefs.ble_pin != 0) {
      _active_ble_pin = _prefs.ble_pin;  // restore persisted pin (BLE_PIN_CODE not set in uni builds)
    } else {
      StdRNG rng;
      _active_ble_pin = rng.nextInt(100000, 999999);
      // auto mode: new random each boot, not persisted
    }
  }
  _ble_iface.begin(BLE_NAME_PREFIX, _prefs.node_name, _active_ble_pin);
  if (_wifi_prefs.comms_mode == COMMS_MODE_WIFI && _wifi_prefs.network_count > 0) {
    // Start async WiFi; use BLE as fallback during connection
    _wifi_net_idx = 0;
    _wifi_connecting = true;
    _wifi_connect_start = millis();
    WiFi.begin(_wifi_prefs.networks[0].ssid, _wifi_prefs.networks[0].password);
    startInterface(_ble_iface);
  } else {
    // BLE or USB: start BLE as default
    startInterface(_ble_iface);
  }
}

#endif  // WITH_WIFI_SWITCHING

// To check if there is pending work
bool MyMesh::hasPendingWork() const {
  return _mgr->getOutboundTotal() > 0 || dirty_contacts_expiry != 0;
}
