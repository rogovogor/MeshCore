#pragma once

#include <Mesh.h>
#include <Arduino.h>

class VolatileRTCClock : public mesh::RTCClock {
  uint32_t base_time;
  uint64_t accumulator;
  unsigned long prev_millis;
  bool time_was_set;
public:
  VolatileRTCClock() { base_time = 1715770351; accumulator = 0; prev_millis = millis(); time_was_set = false; }   // 15 May 2024, 8:50pm
  uint32_t getCurrentTime() override { return base_time + accumulator/1000; }
  void setCurrentTime(uint32_t time) override { base_time = time; accumulator = 0; prev_millis = millis(); time_was_set = true; }

  // True once the clock has been given a real time (by GPS, CLI, the companion
  // app, or a restored/quorum-agreed timestamp). Until then getCurrentTime()
  // only returns the built-in placeholder plus uptime, which must not be
  // presented to the mesh as a trustworthy wall clock.
  bool isTimeReliable() const override { return time_was_set; }

  void tick() override {
    unsigned long now = millis();
    accumulator += (now - prev_millis);
    prev_millis = now;
  }
};

class ArduinoMillis : public mesh::MillisecondClock {
public:
  unsigned long getMillis() override { return millis(); }
};

class StdRNG : public mesh::RNG {
public:
  void begin(long seed) { randomSeed(seed); }
  void random(uint8_t* dest, size_t sz) override {
    for (int i = 0; i < sz; i++) {
      dest[i] = (::random(0, 256) & 0xFF);
    }
  }
};
