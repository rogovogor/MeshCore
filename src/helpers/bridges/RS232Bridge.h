#pragma once

#include "helpers/AbstractBridge.h"
#include "helpers/SimpleMeshTables.h"
#include <Stream.h>

#ifdef WITH_RS232_BRIDGE

/**
 * @brief Bridge implementation using RS232/UART protocol for packet transport
 *
 * This bridge enables mesh packet transport over serial/UART connections,
 * allowing nodes to communicate over wired serial links. It implements a simple
 * packet framing protocol with checksums for reliable transfer.
 *
 * Features:
 * - Point-to-point communication over hardware UART
 * - Fletcher-16 checksum for data integrity verification 
 * - Magic header for packet synchronization
 * - Configurable RX/TX pins via build defines
 * - Baud rate fixed at 115200
 *
 * Packet Structure:
 * [2 bytes] Magic Header - Used to identify start of packet
 * [2 bytes] Fletcher-16 checksum - Data integrity check
 * [1 byte]  Payload length
 * [n bytes] Packet payload
 *
 * The Fletcher-16 checksum is used to validate packet integrity and detect
 * corrupted or malformed packets. It provides error detection capabilities
 * suitable for serial communication where noise or timing issues could
 * corrupt data.
 *
 * Configuration:
 * - Define WITH_RS232_BRIDGE to enable this bridge
 * - Define WITH_RS232_BRIDGE_RX with the RX pin number
 * - Define WITH_RS232_BRIDGE_TX with the TX pin number
 *
 * Platform Support:
 * - ESP32 targets
 * - NRF52 targets
 * - RP2040 targets 
 * - STM32 targets
 */
class RS232Bridge : public AbstractBridge {
public:
  /**
   * @brief Constructs an RS232Bridge instance
   *
   * @param serial The hardware serial port to use
   * @param mgr PacketManager for allocating and queuing packets
   * @param rtc RTCClock for timestamping debug messages
   */
  RS232Bridge(Stream& serial, mesh::PacketManager* mgr, mesh::RTCClock* rtc);

  /**
   * Initializes the RS232 bridge
   * 
   * - Configures UART pins based on platform
   * - Sets baud rate to 115200
   */
  void begin() override;

  /**
   * @brief Main loop handler
   * Processes incoming serial data and builds packets
   */
  void loop() override;

  /**
   * @brief Called when a packet needs to be transmitted over serial
   * Formats and sends the packet with proper framing
   *
   * @param packet The mesh packet to transmit
   */
  void onPacketTransmitted(mesh::Packet* packet) override;

  /**
   * @brief Called when a complete packet has been received from serial
   * Queues the packet for mesh processing if checksum is valid
   * 
   * @param packet The received mesh packet
   */
  void onPacketReceived(mesh::Packet* packet) override;

private:
  /** Helper function to get formatted timestamp for logging */
  const char* getLogDateTime();

  /** Magic number to identify start of RS232 packets */
  static constexpr uint16_t SERIAL_PKT_MAGIC = 0xC03E;

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
  mesh::RTCClock* _rtc;
  SimpleMeshTables _seen_packets;
  uint8_t _rx_buffer[MAX_SERIAL_PACKET_SIZE]; // Buffer for serial data
  uint16_t _rx_buffer_pos = 0;
};

#endif
