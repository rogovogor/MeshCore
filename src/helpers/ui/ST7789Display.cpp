#include "ST7789Display.h"

bool ST7789Display::i2c_probe(TwoWire& wire, uint8_t addr) {
  return true;
/*
  wire.beginTransmission(addr);
  uint8_t error = wire.endTransmission();
  return (error == 0);
*/
}

bool ST7789Display::begin() {
  if(!_isOn) {
    pinMode(PIN_TFT_VDD_CTL, OUTPUT);
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
    digitalWrite(PIN_TFT_RST, HIGH);

    display.init(135, 240);
    display.setRotation(2);
    display.setSPISpeed(40000000);
    display.fillScreen(ST77XX_BLACK);
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(2);
    display.cp437(true);         // Use full 256 char 'Code Page 437' font
  
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
  display.fillScreen(ST77XX_BLACK);
}

void ST7789Display::startFrame(Color bkg) {
  display.fillScreen(0x00);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(2);
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
}

void ST7789Display::setTextSize(int sz) {
  display.setTextSize(sz);
}

void ST7789Display::setColor(Color c) {
  _color = (c == LIGHT) ? ST77XX_WHITE : ST77XX_BLACK;
  display.setTextColor(_color);
}

void ST7789Display::setCursor(int x, int y) {
  display.setCursor(x, y);
}

void ST7789Display::print(const char* str) {
  display.print(str);
}

void ST7789Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x, y, w, h, _color);
}

void ST7789Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x, y, w, h, _color);
}

void ST7789Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display.drawBitmap(x, y, bits, w, h, ST77XX_BLUE);
}

void ST7789Display::endFrame() {
  // display.display();
}
