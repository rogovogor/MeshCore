#include "E213Display.h"

#include "../../MeshCore.h"

#ifdef EINK_RU
  #include "glcdfont6x8.h"
#endif

BaseDisplay* E213Display::detectEInk()
{
    // Test 1: Logic of BUSY pin

    // Determines controller IC manufacturer
    // Fitipower: busy when LOW
    // Solomon Systech: busy when HIGH

    // Force display BUSY by holding reset pin active
    pinMode(DISP_RST, OUTPUT);
    digitalWrite(DISP_RST, LOW);

    delay(10);

    // Read whether pin is HIGH or LOW while busy
    pinMode(DISP_BUSY, INPUT);
    bool busyLogic = digitalRead(DISP_BUSY);

    // Test complete. Release pin
    pinMode(DISP_RST, INPUT);

    if (busyLogic == LOW) {
#ifdef VISION_MASTER_E213
      return new EInkDisplay_VisionMasterE213 ;
#else
      return new EInkDisplay_WirelessPaperV1_1 ;
#endif
    } else {// busy HIGH
#ifdef VISION_MASTER_E213
      return new EInkDisplay_VisionMasterE213V1_1 ;
#else
      return new EInkDisplay_WirelessPaperV1_1_1 ;
#endif
    }
}

bool E213Display::begin() {
  if (_init) return true;

  powerOn();
  if(display==NULL) {
    display = detectEInk();
  }
  display->begin();
  // Set to landscape mode rotated 180 degrees
  display->setRotation(3);

  _init = true;
  _isOn = true;

  clear();
  display->fastmodeOn(); // Enable fast mode for quicker (partial) updates

  return true;
}

void E213Display::powerOn() {
  if (_periph_power) {
    _periph_power->claim();
  } else {
#ifdef PIN_VEXT_EN
    pinMode(PIN_VEXT_EN, OUTPUT);
#ifdef PIN_VEXT_EN_ACTIVE
    digitalWrite(PIN_VEXT_EN, PIN_VEXT_EN_ACTIVE);
#else
    digitalWrite(PIN_VEXT_EN, LOW); // Active low
#endif
#endif
  }
  delay(50);                      // Allow power to stabilize
}

void E213Display::powerOff() {
  if (_periph_power) {
    _periph_power->release();
  } else {
#ifdef PIN_VEXT_EN
#ifdef PIN_VEXT_EN_ACTIVE
    digitalWrite(PIN_VEXT_EN, !PIN_VEXT_EN_ACTIVE);
#else
    digitalWrite(PIN_VEXT_EN, HIGH); // Turn off power
#endif
#endif
  }
}

void E213Display::turnOn() {
  if (!_init) begin();
  else if (!_isOn) {
    powerOn();
    display->fastmodeOn();  // Reinitialize display controller after power was cut
  }
  _isOn = true;
}

void E213Display::turnOff() {
  if (_isOn) {
    powerOff();
    _isOn = false;
  }
}

void E213Display::clear() {
  display->clear();
}

void E213Display::startFrame(Color bkg) {
  display_crc.reset();

  // Fill screen with white first to ensure clean background
  display->fillRect(0, 0, width(), height(), WHITE);

  if (bkg == LIGHT) {
    // Fill with black if light background requested (inverted for e-ink)
    display->fillRect(0, 0, width(), height(), BLACK);
  }
#ifdef EINK_RU
  display->setFont(&glcdfont6x8);
#endif
}

void E213Display::setTextSize(int sz) {
  display_crc.update<int>(sz);
#ifdef EINK_RU
  _size = sz;
#endif
  display->setTextSize(sz);
}

void E213Display::setColor(Color c) {
  display_crc.update<Color>(c);
  // implemented in individual display methods
}

void E213Display::setCursor(int x, int y) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
#ifdef EINK_RU
  _cursor_y_raw = y;
  // GFXfont positions cursor at text baseline, not top-left.
  // Shift down by (size * font_ascent) so callers use top-left semantics.
  display->setCursor(x, y + (_size * 7));
#else
  display->setCursor(x, y);
#endif
}

void E213Display::printWordWrap(const char* str, int max_width) {
#ifdef EINK_RU
  // glcdfont6x8 is monospace: all glyphs are 6 px wide (×_size for textsize scaling)
  const int char_w = 6 * _size;
  const int line_h = 8 * _size;
  const int max_chars = max_width / char_w;

  int len = (int)strlen(str);
  int pos = 0;
  char line_buf[64];  // one line: max ~41 chars + null

  while (pos < len && _cursor_y_raw < height()) {
    if (len - pos <= max_chars) {
      print(str + pos);
      break;
    }

    // find last space within max_chars to avoid mid-word breaks
    int break_at = pos + max_chars;
    for (int i = pos + max_chars; i > pos; i--) {
      if (str[i] == ' ') { break_at = i; break; }
    }

    int seg_len = break_at - pos;
    if (seg_len > (int)sizeof(line_buf) - 1) seg_len = sizeof(line_buf) - 1;
    memcpy(line_buf, str + pos, seg_len);
    line_buf[seg_len] = 0;
    print(line_buf);

    // skip the space separator
    pos = break_at + (str[break_at] == ' ' ? 1 : 0);

    // advance to the next line
    _cursor_y_raw += line_h;
    if (_cursor_y_raw < height()) {
      setCursor(0, _cursor_y_raw);
    }
  }
#else
  print(str);
#endif
}

void E213Display::print(const char *str) {
  display_crc.update<char>(str, strlen(str));
    display->print(str);
}

void E213Display::fillRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
    display->fillRect(x, y, w, h, BLACK);
}

void E213Display::drawRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
    display->drawRect(x, y, w, h, BLACK);
}

void E213Display::drawXbm(int x, int y, const uint8_t *bits, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display_crc.update<uint8_t>(bits, w * h / 8);

  // Width in bytes for bitmap processing
  uint16_t widthInBytes = (w + 7) / 8;

  // Process the bitmap row by row
  for (int by = 0; by < h; by++) {
    // Scan across the row bit by bit
    for (int bx = 0; bx < w; bx++) {
      // Get the current bit using MSB ordering (like GxEPDDisplay)
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      bool bitSet = bits[byteOffset] & bitMask;

      // If the bit is set, draw the pixel
      if (bitSet) {
          display->drawPixel(x + bx, y + by, BLACK);
      }
    }
  }
}

uint16_t E213Display::getTextWidth(const char *str) {
  int16_t x1, y1;
  uint16_t w, h;
  display->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void E213Display::endFrame() {
  uint32_t crc = display_crc.finalize();
  if (crc != last_display_crc_value) {
    display->update();
    last_display_crc_value = crc;
  }
}
