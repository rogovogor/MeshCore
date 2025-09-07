#include "ESPNowBridge.h"

#include <RTClib.h>
#include <WiFi.h>
#include <esp_wifi.h>

#ifdef WITH_ESPNOW_BRIDGE

// Static member to handle callbacks
ESPNowBridge *ESPNowBridge::_instance = nullptr;

// Static callback wrappers
void ESPNowBridge::recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
  if (_instance) {
    _instance->onDataRecv(mac, data, len);
  }
}

void ESPNowBridge::send_cb(const uint8_t *mac, esp_now_send_status_t status) {
  if (_instance) {
    _instance->onDataSent(mac, status);
  }
}

// Fletcher16 checksum calculation
static uint16_t fletcher16(const uint8_t *data, size_t len) {
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;

  while (len--) {
    sum1 = (sum1 + *data++) % 255;
    sum2 = (sum2 + sum1) % 255;
  }

  return (sum2 << 8) | sum1;
}

ESPNowBridge::ESPNowBridge(mesh::PacketManager *mgr, mesh::RTCClock *rtc)
    : _mgr(mgr), _rtc(rtc), _rx_buffer_pos(0) {
  _instance = this;
}

void ESPNowBridge::begin() {
  // Initialize WiFi in station mode
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.printf("%s: ESPNOW BRIDGE: Error initializing ESP-NOW\n", getLogDateTime());
    return;
  }

  // Register callbacks
  esp_now_register_recv_cb(recv_cb);
  esp_now_register_send_cb(send_cb);

  // Add broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memset(&peerInfo, 0, sizeof(peerInfo));
  memset(peerInfo.peer_addr, 0xFF, ESP_NOW_ETH_ALEN); // Broadcast address
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.printf("%s: ESPNOW BRIDGE: Failed to add broadcast peer\n", getLogDateTime());
    return;
  }
}

void ESPNowBridge::loop() {
  // Nothing to do here - ESP-NOW is callback based
}

void ESPNowBridge::xorCrypt(uint8_t *data, size_t len) {
  size_t keyLen = strlen(_secret);
  for (size_t i = 0; i < len; i++) {
    data[i] ^= _secret[i % keyLen];
  }
}

void ESPNowBridge::onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  // Ignore packets that are too small
  if (len < 3) {
#if MESH_PACKET_LOGGING
    Serial.printf("%s: ESPNOW BRIDGE: RX packet too small, len=%d\n", getLogDateTime(), len);
#endif
    return;
  }

  // Check packet header magic
  if (data[0] != ESPNOW_HEADER_MAGIC) {
#if MESH_PACKET_LOGGING
    Serial.printf("%s: ESPNOW BRIDGE: RX invalid magic 0x%02X\n", getLogDateTime(), data[0]);
#endif
    return;
  }

  // Make a copy we can decrypt
  uint8_t decrypted[MAX_ESPNOW_PACKET_SIZE];
  memcpy(decrypted, data + 1, len - 1); // Skip magic byte

  // Try to decrypt
  xorCrypt(decrypted, len - 1);

  // Validate checksum
  uint16_t received_checksum = (decrypted[0] << 8) | decrypted[1];
  uint16_t calculated_checksum = fletcher16(decrypted + 2, len - 3);

  if (received_checksum != calculated_checksum) {
    // Failed to decrypt - likely from a different network
#if MESH_PACKET_LOGGING
    Serial.printf("%s: ESPNOW BRIDGE: RX checksum mismatch, rcv=0x%04X calc=0x%04X\n", getLogDateTime(),
                  received_checksum, calculated_checksum);
#endif
    return;
  }

#if MESH_PACKET_LOGGING
  Serial.printf("%s: ESPNOW BRIDGE: RX, len=%d\n", getLogDateTime(), len - 3);
#endif

  // Create mesh packet
  mesh::Packet *pkt = _instance->_mgr->allocNew();
  if (!pkt) return;

  if (pkt->readFrom(decrypted + 2, len - 3)) {
    _instance->onPacketReceived(pkt);
  } else {
    _instance->_mgr->free(pkt);
  }
}

void ESPNowBridge::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Could add transmission error handling here if needed
}

void ESPNowBridge::onPacketReceived(mesh::Packet *packet) {
  if (!_seen_packets.hasSeen(packet)) {
    _mgr->queueInbound(packet, 0);
  } else {
    _mgr->free(packet);
  }
}

void ESPNowBridge::onPacketTransmitted(mesh::Packet *packet) {
  if (!_seen_packets.hasSeen(packet)) {
    uint8_t buffer[MAX_ESPNOW_PACKET_SIZE];
    buffer[0] = ESPNOW_HEADER_MAGIC;

    // Write packet to buffer starting after magic byte and checksum
    uint16_t len = packet->writeTo(buffer + 3);

    // Calculate and add checksum
    uint16_t checksum = fletcher16(buffer + 3, len);
    buffer[1] = (checksum >> 8) & 0xFF;
    buffer[2] = checksum & 0xFF;

    // Encrypt payload (not including magic byte)
    xorCrypt(buffer + 1, len + 2);

    // Broadcast using ESP-NOW
    uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    esp_err_t result = esp_now_send(broadcastAddress, buffer, len + 3);

#if MESH_PACKET_LOGGING
    if (result == ESP_OK) {
      Serial.printf("%s: ESPNOW BRIDGE: TX, len=%d\n", getLogDateTime(), len);
    } else {
      Serial.printf("%s: ESPNOW BRIDGE: TX FAILED!\n", getLogDateTime());
    }
#endif
  }
}

const char *ESPNowBridge::getLogDateTime() {
  static char tmp[32];
  uint32_t now = _rtc->getCurrentTime();
  DateTime dt = DateTime(now);
  sprintf(tmp, "%02d:%02d:%02d - %d/%d/%d U", dt.hour(), dt.minute(), dt.second(), dt.day(), dt.month(),
          dt.year());
  return tmp;
}

#endif
