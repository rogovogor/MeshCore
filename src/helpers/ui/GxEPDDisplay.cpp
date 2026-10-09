
#include "GxEPDDisplay.h"
#ifdef CYRILLIC_SUPPORT
  #include "glcdfont6x8.h"
#endif

#ifdef EXP_PIN_BACKLIGHT
  #include <PCA9557.h>
  extern PCA9557 expander;
#endif

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 3
#endif

#ifndef EINK_FULL_REFRESH_INTERVAL
  #define EINK_FULL_REFRESH_INTERVAL  100
#endif

// Deghosting on the async path runs on a timer, not on a count of partial
// updates: what a partial-only run costs the image is elapsed time, not update
// count. The count-based EINK_FULL_REFRESH_INTERVAL above still governs the
// synchronous path.
#if defined(WITH_ASYNC_EINK)
#ifndef EINK_FULL_REFRESH_MILLIS
  #define EINK_FULL_REFRESH_MILLIS  (30UL * 60 * 1000)
#endif
#endif

#ifdef ESP32
  // У модулей Vision Master радио сидит на FSPI, а панель на HSPI, поэтому
  // вариант называет свою шину вместо значения по умолчанию.
  #ifndef EINK_SPI_BUS
    #define EINK_SPI_BUS FSPI
  #endif
  SPIClass SPI1 = SPIClass(EINK_SPI_BUS);
#endif

// Color scheme
ColorVal UIColor::window_bkg = GxEPD_WHITE;
ColorVal UIColor::title_bkg = GxEPD_WHITE;
ColorVal UIColor::title_txt = GxEPD_BLACK;
ColorVal UIColor::primary_txt = GxEPD_BLACK;
ColorVal UIColor::secondary_txt = GxEPD_BLACK;
ColorVal UIColor::warning_txt = GxEPD_BLACK;
ColorVal UIColor::popup_bkg = GxEPD_WHITE;
ColorVal UIColor::popup_txt = GxEPD_BLACK;
ColorVal UIColor::corp_blue = GxEPD_BLACK;


bool GxEPDDisplay::begin() {
#ifdef EINK_SHARED_SPI
  // Radio driver already called SPI.begin() with the correct pins; reuse the bus.
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#else
  display.epd2.selectSPI(SPI1, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  #ifdef ESP32
    SPI1.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS);
  #else
    SPI1.begin();
  #endif
#endif
  display.init(115200, true, 10, false);
  // Use stored _rotation (set via setRotation() before first turnOn()) or compile-time default.
  display.setRotation(_rotation);
  // Update logical dimensions to match the rotation now in effect.
  {
    bool is_landscape = (display.width() >= display.height());
    _w = is_landscape ? _logical_long_dim  : _logical_short_dim;
    _h = is_landscape ? _logical_short_dim : _logical_long_dim;
  }
  setTextSize(1);  // Default to size 1
  display.setPartialWindow(0, 0, display.width(), display.height());

  display.fillScreen(GxEPD_WHITE);
  display.display(false);  // full refresh: writes both 0x24 and 0x26 to white, prevents ghost from previous session
  _last_full_refresh = millis();
  display.hibernate();     // SSD1680 requires HW RST + reinit before partial refresh after full refresh
  #if DISP_BACKLIGHT
  digitalWrite(DISP_BACKLIGHT, LOW);
  pinMode(DISP_BACKLIGHT, OUTPUT);
  #endif
  _init = true;
  return true;
}

void GxEPDDisplay::setRotation(uint8_t r) {
  _rotation = r;    // persist so begin() uses this value on next init
  display.setRotation(r);
  display.setPartialWindow(0, 0, display.width(), display.height());
  // After rotation, physical width/height may have swapped.
  // Use stored base dimensions (long/short) to compute the correct logical size.
  bool is_landscape = (display.width() >= display.height());
  _w = is_landscape ? _logical_long_dim  : _logical_short_dim;
  _h = is_landscape ? _logical_short_dim : _logical_long_dim;
  // Force a full refresh so the first frame after rotation is rendered cleanly.
  _partial_refresh_count = EINK_FULL_REFRESH_INTERVAL;
#if defined(WITH_ASYNC_EINK)
  _pending_full_refresh = true;
#endif
  last_display_crc_value = -1;
}

void GxEPDDisplay::turnOn() {
#if defined(WITH_ASYNC_EINK)
  _turn_off_pending = false;
#endif
  if (!_init) begin();
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, HIGH);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, HIGH);
#endif
  _isOn = true;
}

void GxEPDDisplay::turnOff() {
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, LOW);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, LOW);
#endif
  _isOn = false;
#if defined(WITH_ASYNC_EINK)
  if (isRefreshBusy()) {
    // Let the in-flight waveform and controller-RAM synchronization finish.
    // service() is called even while the logical display is off.
    _turn_off_pending = true;
    return;
  }
#endif
  display.hibernate();
  _partial_refresh_count = 0;
  _init = false;  // force re-init on next turnOn() after hibernate
}

void GxEPDDisplay::fullRefreshAndHibernate() {
#if defined(WITH_ASYNC_EINK)
  // Shutdown is intentionally synchronous, but never overwrite the sole
  // framebuffer while an asynchronous refresh still depends on it.
  waitRefreshIdle();
#endif
  display.display(false);
  display.hibernate();
}

// These methods do not exist in featureless builds, preserving the original
// DisplayDriver vtable and synchronous code path when WITH_ASYNC_EINK is off.
#if defined(WITH_ASYNC_EINK)
bool GxEPDDisplay::isRefreshBusy() const {
  return _refresh_state != RefreshState::Idle;
}

void GxEPDDisplay::waitRefreshIdle(uint32_t max_ms) {
  uint32_t started = millis();
  while (isRefreshBusy()) {
    service();
    if ((uint32_t)(millis() - started) >= max_ms) {
      // The panel is not coming back. Stop waiting rather than spin for ever:
      // the caller's frame is lost either way, and a live firmware that has
      // given up on the display beats a hung one.
      _refresh_state = RefreshState::Idle;
      _pending_full_refresh = true;
      last_display_crc_value = 0;
      return;
    }
    delay(1);
  }
}

void GxEPDDisplay::service() {
  if (_refresh_state == RefreshState::Refreshing) {
    AsyncEinkPollResult result = display.poll();
    if (result == AsyncEinkPollResult::Busy) return;
    // Idle means the panel has no operation running while this side believes
    // one is: the two have drifted. Left unhandled the state stayed Refreshing
    // for ever — isRefreshBusy() never cleared and every caller waiting on it
    // spun with no way out. Drop to Idle and let the frame be drawn again.
    if (result == AsyncEinkPollResult::Idle) {
      _refresh_state = RefreshState::Idle;
      last_display_crc_value = 0;   // the dropped frame must not be CRC-skipped
      return;
    }
    if (result == AsyncEinkPollResult::TimedOut) {
      // Past even the generous deadline. Do NOT run finishDisplay() or
      // hibernate() here: both push SPI traffic and a deep-sleep command into a
      // controller that may still be driving its waveform. Abandon the frame,
      // leave the panel alone, and force a full refresh next time to clear
      // whatever half-drawn image it was left with.
      _refresh_state = RefreshState::Idle;
      _pending_full_refresh = true;
      last_display_crc_value = 0;
      return;
    }
    if (result == AsyncEinkPollResult::Complete) {
      // This short SPI transfer is the only point after endFrame() that reads
      // the framebuffer. UI rendering remains gated until it completes.
      display.finishDisplay();
      if (_active_refresh_full) {
        _last_full_refresh = millis();
        _pending_full_refresh = false;
        display.hibernate();
        _refresh_state = RefreshState::Idle;
      } else if (display.startPowerOff()) {
        _refresh_state = RefreshState::PoweringOff;
      } else {
        _refresh_state = RefreshState::Idle;
      }
    }
  } else if (_refresh_state == RefreshState::PoweringOff) {
    AsyncEinkPollResult result = display.poll();
    // Power-off issues no further SPI, so a timeout here is only a stuck BUSY
    // line — dropping to Idle is safe and is the only way out. Idle likewise.
    if (result != AsyncEinkPollResult::Busy) {
      _refresh_state = RefreshState::Idle;
    }
  }

  if (_refresh_state == RefreshState::Idle && _turn_off_pending) {
    display.hibernate();
    _init = false;
    _turn_off_pending = false;
  }
}
#endif

void GxEPDDisplay::clear() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display_crc.reset();
}

void GxEPDDisplay::startFrame(ColorVal bkg) {
#if defined(WITH_ASYNC_EINK)
  // The asynchronous path owns a single framebuffer. Most callers defer
  // rendering via isRefreshBusy(), but early boot and emergency paths can call
  // startFrame() directly. Never let those overwrite pixels still needed by
  // finishDisplay()/writeImageAgain().
  waitRefreshIdle();
#endif
  display.fillScreen(bkg);
  display.setTextColor(_curr_color = UIColor::primary_txt);
  display_crc.reset();
#ifdef CYRILLIC_SUPPORT
  display.setFont(&glcdfont6x8);
#endif
}

void GxEPDDisplay::setTextSize(int sz) {
  display_crc.update<int>(sz);
#ifdef CYRILLIC_SUPPORT
  _font_size = sz;
  display.setTextSize(sz);
#else
  switch(sz) {
    case 1:  // Small
      display.setFont(&FreeSans9pt7b);
      break;
    case 2:  // Medium Bold
      display.setFont(&FreeSansBold12pt7b);
      break;
    case 3:  // Large
      display.setFont(&FreeSans18pt7b);
      break;
    default:
      display.setFont(&FreeSans9pt7b);
      break;
  }
#endif
}

void GxEPDDisplay::setColor(ColorVal c) {
  display_crc.update<ColorVal> (c);
  display.setTextColor(_curr_color = c);
}

void GxEPDDisplay::setCursor(int x, int y) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
#ifdef CYRILLIC_SUPPORT
  _cursor_y_raw = y;
  display.setCursor((x+offset_x)*scale_x, (y + (_font_size * 7) + offset_y)*scale_y);
#else
  display.setCursor((x+offset_x)*scale_x, (y+offset_y)*scale_y);
#endif
}

void GxEPDDisplay::print(const char* str) {
#ifdef CYRILLIC_SUPPORT
  char cp[256];
  translateUTF8ToBlocks(cp, str, sizeof(cp));
  str = cp;
#endif
  display_crc.update<char>(str, strlen(str));
  display.print(str);
}

void GxEPDDisplay::fillRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.fillRect(x*scale_x, y*scale_y, w*scale_x, h*scale_y, _curr_color);
}

void GxEPDDisplay::drawRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.drawRect(x*scale_x, y*scale_y, w*scale_x, h*scale_y, _curr_color);
}

void GxEPDDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display_crc.update<uint8_t>(bits, w * h / 8);
  // Calculate the base position in display coordinates
  uint16_t startX = x * scale_x;
  uint16_t startY = y * scale_y;
  
  // Width in bytes for bitmap processing
  uint16_t widthInBytes = (w + 7) / 8;
  
  // Process the bitmap row by row
  for (uint16_t by = 0; by < h; by++) {
    // Calculate the target y-coordinates for this logical row
    int y1 = startY + (int)(by * scale_y);
    int y2 = startY + (int)((by + 1) * scale_y);
    int block_h = y2 - y1;
    
    // Scan across the row bit by bit
    for (uint16_t bx = 0; bx < w; bx++) {
      // Calculate the target x-coordinates for this logical column
      int x1 = startX + (int)(bx * scale_x);
      int x2 = startX + (int)((bx + 1) * scale_x);
      int block_w = x2 - x1;
      
      // Get the current bit
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      bool bitSet = pgm_read_byte(bits + byteOffset) & bitMask;
      
      // If the bit is set, draw a block of pixels
      if (bitSet) {
        // Draw the block as a filled rectangle
        display.fillRect(x1, y1, block_w, block_h, _curr_color);
      }
    }
  }
}

uint16_t GxEPDDisplay::getTextWidth(const char* str) {
#ifdef CYRILLIC_SUPPORT
  char cp[256];
  translateUTF8ToBlocks(cp, str, sizeof(cp));
  str = cp;
#endif
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return ceil((w + 1) / scale_x);
}

void GxEPDDisplay::endFrame() {
  uint32_t crc = display_crc.finalize();
  if (crc != last_display_crc_value) {
#if defined(WITH_ASYNC_EINK)
    // Wrap-safe elapsed check (millis() rolls over every ~49 days). The timer is
    // NOT reset while full refresh is suppressed, so turning Antighost back on
    // deghosts on the next frame if the interval has already passed.
    bool timer_due = (uint32_t)(millis() - _last_full_refresh) >= EINK_FULL_REFRESH_MILLIS;
    bool due = _pending_full_refresh || (timer_due && !_suppress_full_refresh);
    _active_refresh_full = due;
    if (display.startDisplay(!_active_refresh_full)) {
      // Commit the CRC only after the panel accepted the refresh. If startup is
      // ever refused, an identical next frame must be allowed to retry.
      last_display_crc_value = crc;
      _refresh_state = RefreshState::Refreshing;
    }
#else
    last_display_crc_value = crc;
    bool do_full = (++_partial_refresh_count >= EINK_FULL_REFRESH_INTERVAL) && !_suppress_full_refresh;
    if (do_full) {
      display.display(false);  // full refresh (deghost); writeImageAgain() syncs 0x26=0x24
      _partial_refresh_count = 0;
      // SSD1680 requires a controller re-initialisation after full refresh before partial
      // updates work correctly (WeAct reference: Epaper_Initial_partial = HW RST + reinit).
      // GxEPD2 hibernate() puts the panel in deep sleep; on next writeImage() call,
      // _InitDisplay() sees _hibernating=true and performs the required hardware reset.
      display.hibernate();
      // _init stays true — begin() is NOT called, so SPI and framebuffer are preserved.
    } else {
      display.display(true);   // partial refresh
    }
#endif
  }
}
