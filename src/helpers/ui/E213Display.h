#pragma once

#include "DisplayDriver.h"

#include <SPI.h>
#include <Wire.h>
#include <heltec-eink-modules.h>

enum EInkDetectionResult {
    V_LCMEN213EFC1 = 0, // Initial version
    V_E0213A367 = 1,    // E213 PCB marked V1.1 (Mid 2025)
};

// Display driver for E213 e-ink display
class E213Display : public DisplayDriver {
#ifdef VISION_MASTER_E213
  EInkDisplay_VisionMasterE213 display;
  EInkDisplay_VisionMasterE213V1_1 display1;
#else
  EInkDisplay_WirelessPaperV1_1 display;
  EInkDisplay_WirelessPaperV1_1_1 display1;
#endif
  EInkDetectionResult _version =V_LCMEN213EFC1;
  bool _init = false;
  bool _isOn = false;

public:
  E213Display() : DisplayDriver(250, 122) {}

  bool begin();
  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char *str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t *bits, int w, int h) override;
  uint16_t getTextWidth(const char *str) override;
  void endFrame() override;

private:
  EInkDetectionResult detectEInk();
  void powerOn();
  void powerOff();
};