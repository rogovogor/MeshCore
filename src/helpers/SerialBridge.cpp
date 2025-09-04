#include "SerialBridge.h"
#include <HardwareSerial.h>

#ifdef BRIDGE_OVER_SERIAL

#define SERIAL_PKT_MAGIC 0xcafe

struct SerialPacket {
  uint16_t magic, len, crc;
  uint8_t payload[MAX_TRANS_UNIT];
  SerialPacket() : magic(SERIAL_PKT_MAGIC), len(0), crc(0) {}
};

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
    SerialPacket spkt;
    spkt.len = packet->writeTo(spkt.payload);
    spkt.crc = fletcher16(spkt.payload, spkt.len);
    _serial->write((uint8_t *)&spkt, sizeof(SerialPacket));
  }
}

void SerialBridge::loop() {
  while (_serial->available()) {
    onPacketReceived();
  }
}

void SerialBridge::onPacketReceived() {
  static constexpr uint16_t size = sizeof(SerialPacket) + 1;
  static uint8_t buffer[size];
  static uint16_t tail = 0;

  buffer[tail] = (uint8_t)_serial->read();
  tail = (tail + 1) % size;

  // Check for complete packet by looking back to where the magic number should be
  const uint16_t head = (tail - sizeof(SerialPacket) + size) % size;
  if ((buffer[head] | (buffer[(head + 1) % size] << 8)) != SERIAL_PKT_MAGIC) {
    return;
  }

  uint8_t bytes[MAX_TRANS_UNIT];
  const uint16_t len = buffer[(head + 2) % size] | (buffer[(head + 3) % size] << 8);

  if (len == 0 || len > sizeof(bytes)) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    bytes[i] = buffer[(head + 6 + i) % size];
  }

  const uint16_t crc = buffer[(head + 4) % size] | (buffer[(head + 5) % size] << 8);
  const uint16_t f16 = fletcher16(bytes, len);

  if ((f16 != crc)) {
    return;
  }

  mesh::Packet *new_pkt = _mgr->allocNew();
  if (new_pkt == NULL) {
    return;
  }

  new_pkt->readFrom(bytes, len);
  if (!_seen_packets.hasSeen(new_pkt)) {
    _mgr->queueInbound(new_pkt, 0);
  } else {
    _mgr->free(new_pkt);
  }
}

#endif
