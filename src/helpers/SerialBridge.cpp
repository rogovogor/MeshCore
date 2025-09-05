#include "SerialBridge.h"
#include <HardwareSerial.h>

#ifdef BRIDGE_OVER_SERIAL

// Fletcher-16
// https://en.wikipedia.org/wiki/Fletcher%27s_checksum
inline static uint16_t fletcher16(const uint8_t *bytes, const size_t len) {
  uint8_t sum1 = 0, sum2 = 0;

  for (size_t i = 0; i < len; i++) {
    sum1 = (sum1 + bytes[i]) % 255;
    sum2 = (sum2 + sum1) % 255;
  }

  return (sum2 << 8) | sum1;
};

SerialBridge::SerialBridge(Stream& serial, mesh::PacketManager* mgr) : _serial(&serial), _mgr(mgr) {}

void SerialBridge::begin() {
#if defined(ESP32)
  ((HardwareSerial*)_serial)->setPins(BRIDGE_OVER_SERIAL_RX, BRIDGE_OVER_SERIAL_TX);
#elif defined(RP2040_PLATFORM)
  ((HardwareSerial*)_serial)->setRX(BRIDGE_OVER_SERIAL_RX);
  ((HardwareSerial*)_serial)->setTX(BRIDGE_OVER_SERIAL_TX);
#elif defined(STM32_PLATFORM)
  ((HardwareSerial*)_serial)->setRx(BRIDGE_OVER_SERIAL_RX);
  ((HardwareSerial*)_serial)->setTx(BRIDGE_OVER_SERIAL_TX);
#else
#error SerialBridge was not tested on the current platform
#endif
  ((HardwareSerial*)_serial)->begin(115200);
}

void SerialBridge::onPacketTransmitted(mesh::Packet* packet) {
  if (!_seen_packets.hasSeen(packet)) {
    uint8_t buffer[MAX_SERIAL_PACKET_SIZE];
    uint16_t len = packet->writeTo(buffer + 4);

    buffer[0] = (SERIAL_PKT_MAGIC >> 8) & 0xFF;
    buffer[1] = SERIAL_PKT_MAGIC & 0xFF;
    buffer[2] = (len >> 8) & 0xFF;
    buffer[3] = len & 0xFF;

    uint16_t checksum = fletcher16(buffer + 4, len);
    buffer[4 + len] = (checksum >> 8) & 0xFF;
    buffer[5 + len] = checksum & 0xFF;

    _serial->write(buffer, len + SERIAL_OVERHEAD);
  }
}

void SerialBridge::loop() {
  while (_serial->available()) {
    uint8_t b = _serial->read();

    if (_rx_buffer_pos < 2) {
      // Waiting for magic word
      if ((_rx_buffer_pos == 0 && b == ((SERIAL_PKT_MAGIC >> 8) & 0xFF)) ||
          (_rx_buffer_pos == 1 && b == (SERIAL_PKT_MAGIC & 0xFF))) {
        _rx_buffer[_rx_buffer_pos++] = b;
      } else {
        _rx_buffer_pos = 0;
      }
    } else {
      // Reading length, payload, and checksum
      _rx_buffer[_rx_buffer_pos++] = b;

      if (_rx_buffer_pos >= 4) {
        uint16_t len = (_rx_buffer[2] << 8) | _rx_buffer[3];
        if (len > MAX_PACKET_PAYLOAD) {
          _rx_buffer_pos = 0; // Invalid length, reset
          return;
        }

      if (_rx_buffer_pos == len + SERIAL_OVERHEAD) { // Full packet received
        uint16_t checksum = (_rx_buffer[4 + len] << 8) | _rx_buffer[5 + len];
        if (checksum == fletcher16(_rx_buffer + 4, len)) {
          mesh::Packet *pkt = _mgr->allocNew();
            if (pkt) {
              memcpy(pkt->payload, _rx_buffer + 4, len);
              pkt->payload_len = len;
              onPacketReceived(pkt);
            }
          }
          _rx_buffer_pos = 0; // Reset for next packet
        }
      }
    }
  }
}

void SerialBridge::onPacketReceived(mesh::Packet* packet) {
  if (!_seen_packets.hasSeen(packet)) {
    _mgr->queueInbound(packet, 0);
  } else {
    _mgr->free(packet);
  }
}

#endif
