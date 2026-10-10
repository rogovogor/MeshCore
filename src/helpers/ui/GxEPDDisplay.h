#pragma once

#include <SPI.h>
#include <Wire.h>

#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <CRC32.h>

#include "DisplayDriver.h"
#include <helpers/RefCountedDigitalPin.h>

#if defined(WITH_ASYNC_EINK)
  #include "eink_async/AsyncEinkPanels.h"
  // The panel is named once, by EINK_DISPLAY_MODEL; the adapter follows from it,
  // so a config cannot name a panel and an adapter that disagree.
  using SelectedAsyncEinkPanel = typename AsyncEinkPanelFor<EINK_DISPLAY_MODEL>::Type;
#endif

class GxEPDDisplay : public DisplayDriver {

#if defined(EINK_DISPLAY_MODEL)
#if defined(WITH_ASYNC_EINK)
  AsyncEinkCanvas<SelectedAsyncEinkPanel> display;
#else
  GxEPD2_BW<EINK_DISPLAY_MODEL, EINK_DISPLAY_MODEL::HEIGHT> display;
#endif
  const float scale_x  = EINK_SCALE_X; 
  const float scale_y  = EINK_SCALE_Y;
  const float offset_x = EINK_X_OFFSET;
  const float offset_y = EINK_Y_OFFSET;
#else
  GxEPD2_BW<GxEPD2_150_BN, 200> display;
  const float scale_x  = 1.5625f;
  const float scale_y  = 1.5625f;
  const float offset_x = 0;
  const float offset_y = 10;
#endif
  // Logical base dimensions: the "long" and "short" side of the panel.
  // Stored once so setRotation() can derive _w/_h correctly regardless of
  // how many times the orientation is changed.
#if defined(EINK_LOGICAL_W) && defined(EINK_LOGICAL_H)
  int _logical_long_dim  = (EINK_LOGICAL_W >= EINK_LOGICAL_H) ? EINK_LOGICAL_W : EINK_LOGICAL_H;
  int _logical_short_dim = (EINK_LOGICAL_W >= EINK_LOGICAL_H) ? EINK_LOGICAL_H : EINK_LOGICAL_W;
#else
  int _logical_long_dim  = 128;
  int _logical_short_dim = 128;
#endif

  uint8_t _rotation = 3;   // current rotation; persists across turnOff/turnOn so boot uses prefs

  bool _init = false;
  bool _isOn = false;
  uint16_t _curr_color;
  CRC32 display_crc;
  int last_display_crc_value = 0;
  uint8_t _partial_refresh_count = 0;
  uint32_t _last_full_refresh = 0;   // millis() of the last full refresh (deghost timer)
  bool _pending_full_refresh = false;
  bool _suppress_full_refresh = false;

#if defined(WITH_ASYNC_EINK)
  enum class RefreshState : uint8_t { Idle, Refreshing, PoweringOff };
  RefreshState _refresh_state = RefreshState::Idle;
  bool _active_refresh_full = false;
  bool _turn_off_pending = false;
#endif

public:
#if defined(EINK_DISPLAY_MODEL)
  #if defined(EINK_LOGICAL_W)
#if defined(WITH_ASYNC_EINK)
    // GxEPD2 сам не управляет питанием панели, поэтому рельс удерживаем: claim()
    // без release() оставляет питание включённым на всё время работы.
    GxEPDDisplay(RefCountedDigitalPin* periph_power = NULL) : DisplayDriver(EINK_LOGICAL_W, EINK_LOGICAL_H), display(SelectedAsyncEinkPanel(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) { if (periph_power) periph_power->claim(); }
#else
    GxEPDDisplay() : DisplayDriver(EINK_LOGICAL_W, EINK_LOGICAL_H), display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}
#endif
  #else
#if defined(WITH_ASYNC_EINK)
    GxEPDDisplay(RefCountedDigitalPin* periph_power = NULL) : DisplayDriver(128, 128), display(SelectedAsyncEinkPanel(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) { if (periph_power) periph_power->claim(); }
#else
    GxEPDDisplay() : DisplayDriver(128, 128), display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}
#endif
  #endif
#else
  GxEPDDisplay() : DisplayDriver(128, 128), display(GxEPD2_150_BN(DISP_CS, DISP_DC, DISP_RST, DISP_BUSY)) {}
#endif

  bool begin();

  void setRotation(uint8_t r) override;
  bool isEink() const override { return true; }
  void setFullRefreshSuppressed(bool s) override { _suppress_full_refresh = s; }
  void forceFullRefresh() override { _pending_full_refresh = true; }
  void fullRefreshAndHibernate() override;
#if defined(WITH_ASYNC_EINK)
  void service() override;
  bool isRefreshBusy() const override;
  // Pump service() until the panel is idle, or the deadline passes. Every
  // caller that has to block on the panel goes through this: the open-coded
  // `while (isRefreshBusy()) { service(); delay(1); }` had no way out, so a
  // panel that never released BUSY hung the firmware outright.
  void waitRefreshIdle(uint32_t max_ms = 12000) override;
#endif
  bool isOn() override { return _isOn; }
  bool isEink() override { return true; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
