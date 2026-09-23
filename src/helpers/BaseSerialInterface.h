#pragma once

#include <Arduino.h>

#define MAX_FRAME_SIZE  176   // +4 for transport codes (region scoping)

class BaseSerialInterface {
protected:
  BaseSerialInterface() { }

public:
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual bool isEnabled() const = 0;

  virtual bool isConnected() const = 0;
  virtual void disconnect() {}
  virtual void loop() {};

  virtual bool isWriteBusy() const = 0;
  virtual size_t writeFrame(const uint8_t src[], size_t len) = 0;
  virtual size_t checkRecvFrame(uint8_t dest[]) = 0;
  // Best-effort push of one already queued frame, without waiting for the next
  // checkRecvFrame(). Transports that write straight through do nothing here.
  virtual void flushSend() { }
  // True while frames are queued but not yet handed to the transport. Always
  // false where writeFrame() writes straight through.
  virtual bool hasPendingSend() const { return false; }
};
