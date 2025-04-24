#ifdef ST7789

#include "ST7789Display.h"

bool ST7789Display::begin() {
  if(!_isOn) {
    pinMode(PIN_TFT_VDD_CTL, OUTPUT);
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
    digitalWrite(PIN_TFT_RST, HIGH);

    display.init();
    display.displayOn();
  
    _isOn = true;
  }
  return true;
}

void ST7789Display::turnOn() {
  ST7789Display::begin();
}

void ST7789Display::turnOff() {
  digitalWrite(PIN_TFT_VDD_CTL, HIGH);
  digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
  digitalWrite(PIN_TFT_RST, LOW);
  // digitalWrite(PIN_TFT_VDD_CTL, LOW);
  // digitalWrite(PIN_TFT_LEDA_CTL, LOW);
  _isOn = false;
}

void ST7789Display::clear() {
  // display.fillScreen(ST77XX_BLACK);
  display.clear();
}

void ST7789Display::startFrame(Color bkg) {
  display.clear();
  // display.fillScreen(0x00);
  // display.setTextColor(ST77XX_WHITE);
  // display.setTextSize(2);
  // display.cp437(true);         // Use full 256 char 'Code Page 437' font
}

void ST7789Display::setTextSize(int sz) {
//  display.setTextSize(sz);
  switch(sz) {
    case 1 :
      display.setFont(ArialMT_Plain_10);
      break;
    case 2 :
      display.setFont(ArialMT_Plain_16);
      break;
    case 3 :
      display.setFont(ArialMT_Plain_24);
      break;
    default:
      display.setFont(ArialMT_Plain_10);
  }
}

void ST7789Display::setColor(Color c) {
  switch (c) {
    case DisplayDriver::DARK :
      _color = ST77XX_BLACK;
      break;
    case DisplayDriver::LIGHT : 
      _color = ST77XX_WHITE;
      break;
    case DisplayDriver::RED : 
      _color = ST77XX_RED;
      break;
    case DisplayDriver::GREEN : 
      _color = ST77XX_GREEN;
      break;
    case DisplayDriver::BLUE : 
      _color = ST77XX_BLUE;
      break;
    case DisplayDriver::YELLOW : 
      _color = ST77XX_YELLOW;
      break;
    case DisplayDriver::ORANGE : 
      _color = ST77XX_ORANGE;
      break;
    default:
      _color = ST77XX_WHITE;
      break;
  }
  display.setRGB(_color);
  //display.setColor((OLEDDISPLAY_COLOR) 4);
}

void ST7789Display::setCursor(int x, int y) {
  //display.setCursor(x, y);
  _x = x;
  _y = y;
}

void ST7789Display::print(const char* str) {
  display.drawString(_x, _y, str);
}

void ST7789Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x, y, w, h);
}

void ST7789Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x, y, w, h);
}

void ST7789Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display.drawXbm(x, y, w, h, bits);
}

void ST7789Display::endFrame() {
  display.display();
}

#endif