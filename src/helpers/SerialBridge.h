#pragma once

#include "helpers/AbstractBridge.h"
#include "helpers/SimpleMeshTables.h"
#include <Stream.h>

#ifdef BRIDGE_OVER_SERIAL

/**
 * @brief A bridge implementation that uses a serial port to connect two mesh networks.
 */
class SerialBridge : public AbstractBridge {
public:
  /**
   * @brief Construct a new Serial Bridge object
   * 
   * @param serial The serial port to use for the bridge.
   * @param mgr A pointer to the packet manager.
   */
  SerialBridge(Stream& serial, mesh::PacketManager* mgr);
  void begin() override;
  void loop() override;
  void onPacketTransmitted(mesh::Packet* packet) override;
  void onPacketReceived(mesh::Packet* packet) override;

private:
  /**
   * @brief The 2-byte magic word used to signify the start of a packet.
   */
  static constexpr uint16_t SERIAL_PKT_MAGIC = 0xCAFE;

  /**
   * @brief The total overhead of the serial protocol in bytes.
   *        [MAGIC_WORD (2 bytes)] [LENGTH (2 bytes)] [PAYLOAD (variable)] [CHECKSUM (2 bytes)]
   */
  static constexpr uint16_t SERIAL_OVERHEAD = 6;

  /**
   * @brief The maximum size of a packet on the serial line.
   *
   * This is calculated as the sum of:
   * - 1 byte for the packet header (from mesh::Packet)
   * - 4 bytes for transport codes (from mesh::Packet)
   * - 1 byte for the path length (from mesh::Packet)
   * - MAX_PATH_SIZE for the path itself (from MeshCore.h)
   * - MAX_PACKET_PAYLOAD for the payload (from MeshCore.h)
   * - SERIAL_OVERHEAD for the serial framing
   */
  static constexpr uint16_t MAX_SERIAL_PACKET_SIZE = (MAX_TRANS_UNIT + 1) + SERIAL_OVERHEAD;

  Stream* _serial;
  mesh::PacketManager* _mgr;
  SimpleMeshTables _seen_packets;
  uint8_t _rx_buffer[MAX_SERIAL_PACKET_SIZE]; // Buffer for serial data
  uint16_t _rx_buffer_pos = 0;
};

#endif
