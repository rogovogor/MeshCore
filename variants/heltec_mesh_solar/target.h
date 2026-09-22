#pragma once
// POSIX timezone string for the clock display page.
// Examples: "UTC0"           UTC
//           "MSK-3"          Moscow (UTC+3)
//           "EET-2"          Eastern Europe (UTC+2)
//           "CET-1CEST,M3.5.0,M10.5.0/3"  Central Europe with DST
// Override in platformio.ini: -DDISPLAY_TZ="MSK-3"
// See: https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
#ifndef DISPLAY_TZ
#  define DISPLAY_TZ  "UTC0"
#endif


#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <MeshSolarBoard.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/LocationProvider.h>
#include <helpers/ExternalWatchdogManager.h>
#ifdef DISPLAY_CLASS
  #include <helpers/ui/ST7789Display.h>
#endif

class SolarSensorManager : public SensorManager {
  bool gps_active = false;
  bool gps_detected = false;
  LocationProvider* _location;

  void start_gps();
  void stop_gps();
public:
  SolarSensorManager(LocationProvider &location): _location(&location) { }
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  void loop() override;
  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
};

class SolarExternalWatchdog : public ExternalWatchdogManager {
public:
    SolarExternalWatchdog() {}
    bool begin() override;
    void loop() override;
    unsigned long getIntervalMs() const override;
    void feed() override;
};

extern MeshSolarBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern SolarSensorManager sensors;
extern SolarExternalWatchdog external_watchdog;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();
