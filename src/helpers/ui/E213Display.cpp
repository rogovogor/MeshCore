#include "E213Display.h"

#include "../../MeshCore.h"

EInkDetectionResult E213Display::detectEInk()
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

    if (busyLogic == LOW)
        return V_LCMEN213EFC1;
    else // busy HIGH
        return V_E0213A367;
}


bool E213Display::begin() {
  if (_init) return true;

  powerOn();
  _version = detectEInk();
  if(_version==V_LCMEN213EFC1) {
    display.begin();
    // Set to landscape mode rotated 180 degrees
    display.setRotation(3);
  } else{
    display1.begin();
    // Set to landscape mode rotated 180 degrees
    display1.setRotation(3);
  }


  _init = true;
  _isOn = true;

  clear();
  if(_version==V_LCMEN213EFC1) {
    display.fastmodeOn(); // Enable fast mode for quicker (partial) updates
  } else{
    display1.fastmodeOn(); // Enable fast mode for quicker (partial) updates
  }

  return true;
}

void E213Display::powerOn() {
#ifdef PIN_VEXT_EN
  pinMode(PIN_VEXT_EN, OUTPUT);
#ifdef PIN_VEXT_EN_ACTIVE
  digitalWrite(PIN_VEXT_EN, PIN_VEXT_EN_ACTIVE); 
#else
  digitalWrite(PIN_VEXT_EN, LOW); // Active low
#endif
  delay(50);                      // Allow power to stabilize
#endif
}

void E213Display::powerOff() {
#ifdef PIN_VEXT_EN
#ifdef PIN_VEXT_EN_ACTIVE
  digitalWrite(PIN_VEXT_EN, !PIN_VEXT_EN_ACTIVE); 
#else
  digitalWrite(PIN_VEXT_EN, HIGH); // Turn off power
#endif
#endif
}

void E213Display::turnOn() {
  if (!_init) begin();
  powerOn();
  _isOn = true;
}

void E213Display::turnOff() {
  powerOff();
  _isOn = false;
}

void E213Display::clear() {
  if(_version==V_LCMEN213EFC1) {
    display.clear();
  } else{
    display1.clear();
  }
}

void E213Display::startFrame(Color bkg) {
  // Fill screen with white first to ensure clean background
  if(_version==V_LCMEN213EFC1) {
    display.fillRect(0, 0, width(), height(), WHITE);
  } else{
    display1.fillRect(0, 0, width(), height(), WHITE);
  }
  if (bkg == LIGHT) {
    // Fill with black if light background requested (inverted for e-ink)
    if(_version==V_LCMEN213EFC1) {
      display.fillRect(0, 0, width(), height(), BLACK);
    } else{
      display1.fillRect(0, 0, width(), height(), BLACK);
    }
  }
}

void E213Display::setTextSize(int sz) {
  // The library handles text size internally
  if(_version==V_LCMEN213EFC1) {
    display.setTextSize(sz);
  } else{
    display1.setTextSize(sz);
  }
}

void E213Display::setColor(Color c) {
  // implemented in individual display methods
}

void E213Display::setCursor(int x, int y) {
  if(_version==V_LCMEN213EFC1) {
    display.setCursor(x, y);
  } else{
    display1.setCursor(x, y);
  }
}

void E213Display::print(const char *str) {
  if(_version==V_LCMEN213EFC1) {
    display.print(str);
  } else {
    display1.print(str);
  }
}

void E213Display::fillRect(int x, int y, int w, int h) {
  if(_version==V_LCMEN213EFC1) {
    display.fillRect(x, y, w, h, BLACK);
  } else {
    display1.fillRect(x, y, w, h, BLACK);
  }
}

void E213Display::drawRect(int x, int y, int w, int h) {
  if(_version==V_LCMEN213EFC1) {
    display.drawRect(x, y, w, h, BLACK);
  } else {
    display1.drawRect(x, y, w, h, BLACK);
  }
}

void E213Display::drawXbm(int x, int y, const uint8_t *bits, int w, int h) {
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
        if(_version==V_LCMEN213EFC1) {
          display.drawPixel(x + bx, y + by, BLACK);
        } else {
          display1.drawPixel(x + bx, y + by, BLACK);
        }
      }
    }
  }
}

uint16_t E213Display::getTextWidth(const char *str) {
  int16_t x1, y1;
  uint16_t w, h;
  if(_version==V_LCMEN213EFC1) {
    display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  } else {
    display1.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  }
  return w;
}

void E213Display::endFrame() {
  if(_version==V_LCMEN213EFC1) {
    display.update();
  } else {
    display1.update();
  }
}
