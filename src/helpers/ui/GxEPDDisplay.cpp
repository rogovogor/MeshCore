
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

#ifdef ESP32
  SPIClass SPI1 = SPIClass(FSPI);
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
  last_display_crc_value = -1;
}

void GxEPDDisplay::turnOn() {
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
  display.hibernate();
  _partial_refresh_count = 0;
  _init = false;  // force re-init on next turnOn() after hibernate
  _isOn = false;
}

void GxEPDDisplay::clear() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display_crc.reset();
}

void GxEPDDisplay::startFrame(ColorVal bkg) {
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
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return ceil((w + 1) / scale_x);
}

void GxEPDDisplay::endFrame() {
  uint32_t crc = display_crc.finalize();
  if (crc != last_display_crc_value) {
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
  }
}
