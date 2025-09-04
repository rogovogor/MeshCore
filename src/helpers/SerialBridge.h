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
  void onPacketReceived() override;

private:
  Stream* _serial;
  mesh::PacketManager* _mgr;
  SimpleMeshTables _seen_packets;
};

#endif
