
#include "GxEPDDisplay.h"

bool GxEPDDisplay::begin() {
  display.epd2.selectSPI(SPI1, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI1.begin();
  display.init(115200, true, 2, false);
  display.setRotation(3);
  display.setFont(&FreeMono9pt7b);

  display.setPartialWindow(0, 0, display.width(), display.height());

  display.fillScreen(GxEPD_WHITE);
  display.display(true);
  _init = true;
  return true;
}

void GxEPDDisplay::turnOn() {
  if (!_init) begin();
}

void GxEPDDisplay::turnOff() {

}

void GxEPDDisplay::clear() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
}

void GxEPDDisplay::startFrame(Color bkg) {
  display.fillScreen(GxEPD_WHITE);
}

void GxEPDDisplay::setTextSize(int sz) {
}

void GxEPDDisplay::setColor(Color c) {
  display.setTextColor(GxEPD_BLACK);
}

void GxEPDDisplay::setCursor(int x, int y) {
  display.setCursor(x*1.5, (y*1.5)+10);
}

void GxEPDDisplay::print(const char* str) {
  display.print(str);
}

void GxEPDDisplay::fillRect(int x, int y, int w, int h) {
}

void GxEPDDisplay::drawRect(int x, int y, int w, int h) {
}

void GxEPDDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display.drawBitmap(x*1.5, (y*1.5) + 10, bits, w, h, GxEPD_BLACK);
}

void GxEPDDisplay::endFrame() {
  display.display(true);
}
