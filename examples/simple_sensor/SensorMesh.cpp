#include "SensorMesh.h"

/* ------------------------------ Config -------------------------------- */

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

#ifndef ADVERT_NAME
  #define  ADVERT_NAME   "sensor"
#endif
#ifndef ADVERT_LAT
  #define  ADVERT_LAT  0.0
#endif
#ifndef ADVERT_LON
  #define  ADVERT_LON  0.0
#endif

#ifndef ADMIN_PASSWORD
  #define  ADMIN_PASSWORD  "password"
#endif

#ifndef SERVER_RESPONSE_DELAY
  #define SERVER_RESPONSE_DELAY   300
#endif

#ifndef TXT_ACK_DELAY
  #define TXT_ACK_DELAY     200
#endif

#ifndef SENSOR_READ_INTERVAL_SECS
  #define SENSOR_READ_INTERVAL_SECS  60
#endif

/* ------------------------------ Code -------------------------------- */

#define REQ_TYPE_LOGIN               0x00
#define REQ_TYPE_GET_STATUS          0x01
#define REQ_TYPE_KEEP_ALIVE          0x02
#define REQ_TYPE_GET_TELEMETRY_DATA  0x03

#define RESP_SERVER_LOGIN_OK      0   // response to ANON_REQ

#define CLI_REPLY_DELAY_MILLIS  1000

#define LAZY_CONTACTS_WRITE_DELAY       5000

static File openAppend(FILESYSTEM* _fs, const char* fname) {
  #if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    return _fs->open(fname, FILE_O_WRITE);
  #elif defined(RP2040_PLATFORM)
    return _fs->open(fname, "a");
  #else
    return _fs->open(fname, "a", true);
  #endif
}

static File openWrite(FILESYSTEM* _fs, const char* filename) {
  #if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    _fs->remove(filename);
    return _fs->open(filename, FILE_O_WRITE);
  #elif defined(RP2040_PLATFORM)
    return _fs->open(filename, "w");
  #else
    return _fs->open(filename, "w", true);
  #endif
}

void SensorMesh::loadContacts() {
  num_contacts = 0;
  if (_fs->exists("/s_contacts")) {
  #if defined(RP2040_PLATFORM)
    File file = _fs->open("/s_contacts", "r");
  #else
    File file = _fs->open("/s_contacts");
  #endif
    if (file) {
      bool full = false;
      while (!full) {
        ContactInfo c;
        uint8_t pub_key[32];
        uint8_t unused;

        bool success = (file.read(pub_key, 32) == 32);
        success = success && (file.read(&c.type, 1) == 1);
        success = success && (file.read(&c.flags, 1) == 1);
        success = success && (file.read(&unused, 1) == 1);
        success = success && (file.read((uint8_t *)&c.out_path_len, 1) == 1);
        success = success && (file.read(c.out_path, 64) == 64);
        success = success && (file.read(c.shared_secret, PUB_KEY_SIZE) == PUB_KEY_SIZE);
        c.last_timestamp = 0;  // transient
        c.last_activity = 0;

        if (!success) break; // EOF

        c.id = mesh::Identity(pub_key);
        if (num_contacts < MAX_CONTACTS) {
          contacts[num_contacts++] = c;
        } else {
          full = true;
        }
      }
      file.close();
    }
  }
}

void SensorMesh::saveContacts() {
  File file = openWrite(_fs, "/s_contacts");
  if (file) {
    uint8_t unused = 0;

    for (int i = 0; i < num_contacts; i++) {
      auto c = &contacts[i];
      if (c->type == 0) continue;    // don't persist guest contacts

      bool success = (file.write(c->id.pub_key, 32) == 32);
      success = success && (file.write(&c->type, 1) == 1);
      success = success && (file.write(&c->flags, 1) == 1);
      success = success && (file.write(&unused, 1) == 1);
      success = success && (file.write((uint8_t *)&c->out_path_len, 1) == 1);
      success = success && (file.write(c->out_path, 64) == 64);
      success = success && (file.write(c->shared_secret, PUB_KEY_SIZE) == PUB_KEY_SIZE);

      if (!success) break; // write failed
    }
    file.close();
  }
}

uint8_t SensorMesh::handleRequest(bool is_admin, uint32_t sender_timestamp, uint8_t req_type, uint8_t* payload, size_t payload_len) {
  memcpy(reply_data, &sender_timestamp, 4);   // reflect sender_timestamp back in response packet (kind of like a 'tag')

  switch (req_type) {
    case REQ_TYPE_GET_TELEMETRY_DATA: {
      telemetry.reset();
      telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      // query other sensors -- target specific
      sensors.querySensors(0xFF, telemetry);  // allow all telemetry permissions for admin or guest

      uint8_t tlen = telemetry.getSize();
      memcpy(&reply_data[4], telemetry.getBuffer(), tlen);
      return 4 + tlen;  // reply_len
    }
  }
  return 0;  // unknown command
}

mesh::Packet* SensorMesh::createSelfAdvert() {
  uint8_t app_data[MAX_ADVERT_DATA_SIZE];
  uint8_t app_data_len;
  {
    AdvertDataBuilder builder(ADV_TYPE_SENSOR, _prefs.node_name, _prefs.node_lat, _prefs.node_lon);
    app_data_len = builder.encodeTo(app_data);
  }

  return createAdvert(self_id, app_data, app_data_len);
}

ContactInfo* SensorMesh::putContact(const mesh::Identity& id) {
  uint32_t min_time = 0xFFFFFFFF;
  ContactInfo* oldest = &contacts[MAX_CONTACTS - 1];
  for (int i = 0; i < num_contacts; i++) {
    if (id.matches(contacts[i].id)) return &contacts[i];  // already known
    if (!contacts[i].isAdmin() && contacts[i].last_activity < min_time) {
      oldest = &contacts[i];
      min_time = oldest->last_activity;
    }
  }

  ContactInfo* c;
  if (num_contacts < MAX_CONTACTS) {
    c = &contacts[num_contacts++];
  } else {
    c = oldest;  // evict least active contact
  }
  memset(c, 0, sizeof(*c));
  c->id = id;
  c->out_path_len = -1;  // initially out_path is unknown
  return c;
}

void SensorMesh::alertIfLow(Trigger& t, float value, float threshold, const char* text) {
  if (value < threshold) {
    if (!t.triggered) {
      t.triggered = true;
      t.time = getRTCClock()->getCurrentTime();
      sendAlert(text);
    }
  } else {
    if (t.triggered) {
      t.triggered = false;
      // TODO: apply debounce logic
    }
  }
}

void SensorMesh::alertIfHigh(Trigger& t, float value, float threshold, const char* text) {
  if (value > threshold) {
    if (!t.triggered) {
      t.triggered = true;
      t.time = getRTCClock()->getCurrentTime();
      sendAlert(text);
    }
  } else {
    if (t.triggered) {
      t.triggered = false;
      // TODO: apply debounce logic
    }
  }
}

float SensorMesh::getAirtimeBudgetFactor() const {
  return _prefs.airtime_factor;
}

bool SensorMesh::allowPacketForward(const mesh::Packet* packet) {
  if (_prefs.disable_fwd) return false;
  if (packet->isRouteFlood() && packet->path_len >= _prefs.flood_max) return false;
  return true;
}

int SensorMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int) ((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

uint32_t SensorMesh::getRetransmitDelay(const mesh::Packet* packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->path_len + packet->payload_len + 2) * _prefs.tx_delay_factor);
  return getRNG()->nextInt(0, 6)*t;
}
uint32_t SensorMesh::getDirectRetransmitDelay(const mesh::Packet* packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->path_len + packet->payload_len + 2) * _prefs.direct_tx_delay_factor);
  return getRNG()->nextInt(0, 6)*t;
}
int SensorMesh::getInterferenceThreshold() const {
  return _prefs.interference_threshold;
}
int SensorMesh::getAGCResetInterval() const {
  return ((int)_prefs.agc_reset_interval) * 4000;   // milliseconds
}

uint8_t SensorMesh::handleLoginReq(const mesh::Identity& sender, const uint8_t* secret, uint32_t sender_timestamp, const uint8_t* data) {
  bool is_admin;
  if (strcmp((char *) data, _prefs.password) == 0) {  // check for valid password
    is_admin = true;
  } else if (strcmp((char *) data, _prefs.guest_password) == 0) {  // check guest password
    is_admin = false;
  } else {
  #if MESH_DEBUG
    MESH_DEBUG_PRINTLN("Invalid password: %s", &data[4]);
  #endif
    return 0;
  }

  auto client = putContact(sender);  // add to contacts (if not already known)
  if (sender_timestamp <= client->last_timestamp) {
    MESH_DEBUG_PRINTLN("Possible login replay attack!");
    return 0;  // FATAL: client table is full -OR- replay attack
  }

  MESH_DEBUG_PRINTLN("Login success!");
  client->last_timestamp = sender_timestamp;
  client->last_activity = getRTCClock()->getCurrentTime();
  client->type = is_admin ? 1 : 0;
  memcpy(client->shared_secret, secret, PUB_KEY_SIZE);

  if (is_admin) {
    // only need to saveContacts() if this is an admin
    dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  }

  uint32_t now = getRTCClock()->getCurrentTimeUnique();
  memcpy(reply_data, &now, 4);   // response packets always prefixed with timestamp
  reply_data[4] = RESP_SERVER_LOGIN_OK;
  reply_data[5] = 0;  // NEW: recommended keep-alive interval (secs / 16)
  reply_data[6] = client->type;
  reply_data[7] = 0;  // FUTURE: reserved
  getRNG()->random(&reply_data[8], 4);   // random blob to help packet-hash uniqueness

  return 12;  // reply length
}

void SensorMesh::onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret, const mesh::Identity& sender, uint8_t* data, size_t len) {
  if (packet->getPayloadType() == PAYLOAD_TYPE_ANON_REQ) {  // received an initial request by a possible admin client (unknown at this stage)
    uint32_t timestamp;
    memcpy(&timestamp, data, 4);

    data[len] = 0;  // ensure null terminator

    uint8_t req_code;
    uint8_t i = 4;
    if (data[4] < 32) {   // non-print char, is a request code
      req_code = data[i++];
    } else {
      req_code = REQ_TYPE_LOGIN;
    }

    uint8_t reply_len;
    if (req_code == REQ_TYPE_LOGIN) {
      reply_len = handleLoginReq(sender, secret, timestamp, &data[i]);
    } else {
      reply_len = handleRequest(false, timestamp, req_code, &data[i], len - i);
    }

    if (reply_len == 0) return;   // invalid request

    if (packet->isRouteFlood()) {
      // let this sender know path TO here, so they can use sendDirect(), and ALSO encode the response
      mesh::Packet* path = createPathReturn(sender, secret, packet->path, packet->path_len,
                                            PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
      if (path) sendFlood(path, SERVER_RESPONSE_DELAY);
    } else {
      mesh::Packet* reply = createDatagram(PAYLOAD_TYPE_RESPONSE, sender, secret, reply_data, reply_len);
      if (reply) sendFlood(reply, SERVER_RESPONSE_DELAY);
    }
  }
}

int SensorMesh::searchPeersByHash(const uint8_t* hash) {
  int n = 0;
  for (int i = 0; i < num_contacts && n < MAX_SEARCH_RESULTS; i++) {
    if (contacts[i].id.isHashMatch(hash)) {
      matching_peer_indexes[n++] = i;  // store the INDEXES of matching contacts (for subsequent 'peer' methods)
    }
  }
  return n;
}

void SensorMesh::getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) {
  int i = matching_peer_indexes[peer_idx];
  if (i >= 0 && i < num_contacts) {
    // lookup pre-calculated shared_secret
    memcpy(dest_secret, contacts[i].shared_secret, PUB_KEY_SIZE);
  } else {
    MESH_DEBUG_PRINTLN("getPeerSharedSecret: Invalid peer idx: %d", i);
  }
}

void SensorMesh::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) {
  mesh::Mesh::onAdvertRecv(packet, id, timestamp, app_data, app_data_len);  // chain to super impl
#if 0
  // if this a zero hop advert, add it to neighbours
  if (packet->path_len == 0) {
    AdvertDataParser parser(app_data, app_data_len);
    if (parser.isValid() && parser.getType() == ADV_TYPE_REPEATER) {   // just keep neigbouring Repeaters
      putNeighbour(id, timestamp, packet->getSNR());
    }
  }
#endif
}

void SensorMesh::onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx, const uint8_t* secret, uint8_t* data, size_t len) {
  int i = matching_peer_indexes[sender_idx];
  if (i < 0 || i >= num_contacts) {
    MESH_DEBUG_PRINTLN("onPeerDataRecv: Invalid sender idx: %d", i);
    return;
  }

  ContactInfo& from = contacts[i];

  if (type == PAYLOAD_TYPE_REQ) {  // request (from a known contact)
    uint32_t timestamp;
    memcpy(&timestamp, data, 4);

    if (timestamp > from.last_timestamp) {  // prevent replay attacks
      uint8_t reply_len = handleRequest(from.isAdmin(), timestamp, data[4], &data[5], len - 5);
      if (reply_len == 0) return;  // invalid command

      from.last_timestamp = timestamp;
      from.last_activity = getRTCClock()->getCurrentTime();

      if (packet->isRouteFlood()) {
        // let this sender know path TO here, so they can use sendDirect(), and ALSO encode the response
        mesh::Packet* path = createPathReturn(from.id, secret, packet->path, packet->path_len,
                                              PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
        if (path) sendFlood(path, SERVER_RESPONSE_DELAY);
      } else {
        mesh::Packet* reply = createDatagram(PAYLOAD_TYPE_RESPONSE, from.id, secret, reply_data, reply_len);
        if (reply) {
          if (from.out_path_len >= 0) {  // we have an out_path, so send DIRECT
            sendDirect(reply, from.out_path, from.out_path_len, SERVER_RESPONSE_DELAY);
          } else {
            sendFlood(reply, SERVER_RESPONSE_DELAY);
          }
        }
      }
    } else {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: possible replay attack detected");
    }
  } else if (type == PAYLOAD_TYPE_TXT_MSG && len > 5 && from.isAdmin()) {   // a CLI command
    uint32_t sender_timestamp;
    memcpy(&sender_timestamp, data, 4);  // timestamp (by sender's RTC clock - which could be wrong)
    uint flags = (data[4] >> 2);   // message attempt number, and other flags

    if (!(flags == TXT_TYPE_CLI_DATA)) {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: unsupported text type received: flags=%02x", (uint32_t)flags);
    } else if (sender_timestamp > from.last_timestamp) {  // prevent replay attacks
      from.last_timestamp = sender_timestamp;
      from.last_activity = getRTCClock()->getCurrentTime();

      // len can be > original length, but 'text' will be padded with zeroes
      data[len] = 0; // need to make a C string again, with null terminator

      uint8_t temp[166];
      const char *command = (const char *) &data[5];
      char *reply = (char *) &temp[5];
      _cli.handleCommand(sender_timestamp, command, reply);

      int text_len = strlen(reply);
      if (text_len > 0) {
        uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
        if (timestamp == sender_timestamp) {
          // WORKAROUND: the two timestamps need to be different, in the CLI view
          timestamp++;
        }
        memcpy(temp, &timestamp, 4);   // mostly an extra blob to help make packet_hash unique
        temp[4] = (TXT_TYPE_CLI_DATA << 2);

        auto reply = createDatagram(PAYLOAD_TYPE_TXT_MSG, from.id, secret, temp, 5 + text_len);
        if (reply) {
          if (from.out_path_len < 0) {
            sendFlood(reply, CLI_REPLY_DELAY_MILLIS);
          } else {
            sendDirect(reply, from.out_path, from.out_path_len, CLI_REPLY_DELAY_MILLIS);
          }
        }
      }
    } else {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: possible replay attack detected");
    }
  }
}

bool SensorMesh::onPeerPathRecv(mesh::Packet* packet, int sender_idx, const uint8_t* secret, uint8_t* path, uint8_t path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) {
  int i = matching_peer_indexes[sender_idx];
  if (i < 0 || i >= num_contacts) {
    MESH_DEBUG_PRINTLN("onPeerPathRecv: Invalid sender idx: %d", i);
    return false;
  }

  ContactInfo& from = contacts[i];

  MESH_DEBUG_PRINTLN("PATH to contact, path_len=%d", (uint32_t) path_len);
  // NOTE: for this impl, we just replace the current 'out_path' regardless, whenever sender sends us a new out_path.
  // FUTURE: could store multiple out_paths per contact, and try to find which is the 'best'(?)
  memcpy(from.out_path, path, from.out_path_len = path_len);  // store a copy of path, for sendDirect()
  from.last_activity = getRTCClock()->getCurrentTime();

  if (from.isAdmin()) {
    // only need to saveContacts() if this is an admin
    dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  }

  // NOTE: no reciprocal path send!!
  return false;
}

SensorMesh::SensorMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng, mesh::RTCClock& rtc, mesh::MeshTables& tables)
     : mesh::Mesh(radio, ms, rng, rtc, *new StaticPoolPacketManager(32), tables),
      _cli(board, rtc, &_prefs, this), telemetry(MAX_PACKET_PAYLOAD - 4)
{
  num_contacts = 0;
  next_local_advert = next_flood_advert = 0;
  dirty_contacts_expiry = 0;
  last_read_time = 0;

  // defaults
  memset(&_prefs, 0, sizeof(_prefs));
  _prefs.airtime_factor = 1.0;    // one half
  _prefs.rx_delay_base =   0.0f;  // turn off by default, was 10.0;
  _prefs.tx_delay_factor = 0.5f;   // was 0.25f
  StrHelper::strncpy(_prefs.node_name, ADVERT_NAME, sizeof(_prefs.node_name));
  _prefs.node_lat = ADVERT_LAT;
  _prefs.node_lon = ADVERT_LON;
  StrHelper::strncpy(_prefs.password, ADMIN_PASSWORD, sizeof(_prefs.password));
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.advert_interval = 1;  // default to 2 minutes for NEW installs
  _prefs.flood_advert_interval = 3;   // 3 hours
  _prefs.disable_fwd = true;
  _prefs.flood_max = 64;
  _prefs.interference_threshold = 0;  // disabled
}

void SensorMesh::begin(FILESYSTEM* fs) {
  mesh::Mesh::begin();
  _fs = fs;
  // load persisted prefs
  _cli.loadPrefs(_fs);

  loadContacts();

  radio_set_params(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
  radio_set_tx_power(_prefs.tx_power_dbm);

  updateAdvertTimer();
  updateFloodAdvertTimer();
}

bool SensorMesh::formatFileSystem() {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    return InternalFS.format();
#elif defined(RP2040_PLATFORM)
    return LittleFS.format();
#elif defined(ESP32)
    return SPIFFS.format();
#else
    #error "need to implement file system erase"
    return false;
#endif
}

void SensorMesh::sendSelfAdvertisement(int delay_millis) {
  mesh::Packet* pkt = createSelfAdvert();
  if (pkt) {
    sendFlood(pkt, delay_millis);
  } else {
    MESH_DEBUG_PRINTLN("ERROR: unable to create advertisement packet!");
  }
}

void SensorMesh::updateAdvertTimer() {
  if (_prefs.advert_interval > 0) {  // schedule local advert timer
    next_local_advert = futureMillis( ((uint32_t)_prefs.advert_interval) * 2 * 60 * 1000);
  } else {
    next_local_advert = 0;  // stop the timer
  }
}
void SensorMesh::updateFloodAdvertTimer() {
  if (_prefs.flood_advert_interval > 0) {  // schedule flood advert timer
    next_flood_advert = futureMillis( ((uint32_t)_prefs.flood_advert_interval) * 60 * 60 * 1000);
  } else {
    next_flood_advert = 0;  // stop the timer
  }
}

void SensorMesh::setTxPower(uint8_t power_dbm) {
  radio_set_tx_power(power_dbm);
}

void SensorMesh::loop() {
  mesh::Mesh::loop();

  if (next_flood_advert && millisHasNowPassed(next_flood_advert)) {
    mesh::Packet* pkt = createSelfAdvert();
    if (pkt) sendFlood(pkt);

    updateFloodAdvertTimer();   // schedule next flood advert
    updateAdvertTimer();   // also schedule local advert (so they don't overlap)
  } else if (next_local_advert && millisHasNowPassed(next_local_advert)) {
    mesh::Packet* pkt = createSelfAdvert();
    if (pkt) sendZeroHop(pkt);

    updateAdvertTimer();   // schedule next local advert
  }

  uint32_t curr = getRTCClock()->getCurrentTime();
  if (curr >= last_read_time + SENSOR_READ_INTERVAL_SECS) {
    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
    // query other sensors -- target specific
    sensors.querySensors(0xFF, telemetry);  // allow all telemetry permissions

    checkForAlerts();

    // save telemetry to time-series datastore
    File file = openAppend(_fs, "/s_data");
    if (file) {
      file.write((uint8_t *)&curr, 4);  // start record with RTC timestamp
      uint8_t tlen = telemetry.getSize();
      file.write(&tlen, 1);
      file.write(telemetry.getBuffer(), tlen);
      uint8_t zero = 0;
      while (tlen < MAX_PACKET_PAYLOAD - 4) {     // pad with zeroes, for fixed record length
        file.write(&zero, 1);
        tlen++;
      }
      file.close();
    }

    last_read_time = curr;
  }

  // is there are pending dirty contacts write needed?
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    saveContacts();
    dirty_contacts_expiry = 0;
  }
}
