#ifdef ST7789

#include "ST7789Display.h"
#include "glcdfont6x8.h"
#ifdef CYRILLIC_SUPPORT
#include "OLEDDisplayFontsRU.h"
#endif

// Coordinate system
// -----------------
// The display API (DisplayDriver) uses a VIRTUAL canvas of 128×64 px — same as SSD1306 OLED.
// All incoming x/y arguments are virtual. Physical pixels are obtained by:
//   phys_x = virtual_x * SCALE_X + X_OFFSET
//   phys_y = virtual_y * SCALE_Y + Y_OFFSET
//
// T114 (240×135): uniform scale 1.875 keeps aspect ratio square; Y_OFFSET=7 letterboxes
//   the 120px content band vertically (7px top, 8px bottom).
// T190 (320×170): non-uniform scale is acceptable — aspect ratio matches 5:2.66 panel.
//
// Every drawing primitive (fillRect, drawXbm, drawV3ClockString, …) MUST use these macros
// so the coordinate system stays consistent across the whole driver.

#ifndef X_OFFSET
#define X_OFFSET 0
#endif

#ifndef Y_OFFSET
#define Y_OFFSET 1
#endif

#ifdef HELTEC_VISION_MASTER_T190
  #define SCALE_X  2.5f        // 320 / 128
  #define SCALE_Y  2.65625f    // 170 / 64
#else
  #define SCALE_X  1.875f      // 240 / 128
  #define SCALE_Y  1.875f      // uniform — 64 virtual → 120px, centered in 135px
  #undef  Y_OFFSET
  #define Y_OFFSET 7           // (135 - 120) / 2
#endif

#ifdef DISPLAY_SCALE_X
  #define SCALE_X DISPLAY_SCALE_X
#endif

#ifdef DISPLAY_SCALE_Y
  #define SCALE_Y DISPLAY_SCALE_Y
#endif

// Color scheme
ColorVal UIColor::window_bkg = OLEDDISPLAY_COLOR::BLACK;
ColorVal UIColor::title_bkg = OLEDDISPLAY_COLOR::BLACK;
ColorVal UIColor::title_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::primary_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::secondary_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::warning_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::popup_bkg = OLEDDISPLAY_COLOR::BLACK;
ColorVal UIColor::popup_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::corp_blue = OLEDDISPLAY_COLOR::WHITE;

bool ST7789Display::begin() {
  if(!_isOn) {
    pinMode(PIN_TFT_VDD_CTL, OUTPUT);
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
  #ifdef PIN_TFT_LEDA_CTL_ACTIVE
    digitalWrite(PIN_TFT_LEDA_CTL, PIN_TFT_LEDA_CTL_ACTIVE);
  #else
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
  #endif
    digitalWrite(PIN_TFT_RST, HIGH);

    display.init();
    display.landscapeScreen();
    #ifdef DISPLAY_FLIP_VERTICALLY
    display.flipScreenVertically();
    #endif
    display.displayOn();
    setCursor(0,0);

    _isOn = true;
  }
  return true;
}

void ST7789Display::turnOn() {
  if (!_isOn) {
    // Restore power to the display but keep backlight off
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
    digitalWrite(PIN_TFT_RST, HIGH);
    
    // Re-initialize the display
    display.init();
    display.displayOn();
    #ifdef DISPLAY_FLIP_VERTICALLY
    display.flipScreenVertically();
    #endif
    delay(20);

    // Now turn on the backlight
  #ifdef PIN_TFT_LEDA_CTL_ACTIVE
    digitalWrite(PIN_TFT_LEDA_CTL, PIN_TFT_LEDA_CTL_ACTIVE);
  #else
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
  #endif    
    _isOn = true;
  }
}

void ST7789Display::turnOff() {
  digitalWrite(PIN_TFT_VDD_CTL, HIGH);
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
  digitalWrite(PIN_TFT_LEDA_CTL, !PIN_TFT_LEDA_CTL_ACTIVE);
#else
  digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
#endif
  digitalWrite(PIN_TFT_RST, LOW);
  _isOn = false;
}

void ST7789Display::clear() {
  display.clear();
}

void ST7789Display::startFrame(ColorVal bkg) {
  display.clear();  // TODO: use bkg
  _text_scale = 1;
  _use_v3_clock_font = false;
  setColor(UIColor::primary_txt);
#ifdef CYRILLIC_SUPPORT
  display.setFont(ArialMT_Plain_16_RU);
#else
  display.setFont(ArialMT_Plain_16);
#endif
}

void ST7789Display::setTextSize(int sz) {
  _text_scale = 1;
  _use_v3_clock_font = false;
  switch(sz) {
    case 1 :
#ifdef CYRILLIC_SUPPORT
      display.setFont(ArialMT_Plain_16_RU);
#else
      display.setFont(ArialMT_Plain_16);
#endif
      break;
    case 2 :
#ifdef CYRILLIC_SUPPORT
      display.setFont(ArialMT_Plain_24_RU);
#else
      display.setFont(ArialMT_Plain_24);
#endif
      break;
    case 3 :
      // OLEDDisplay cannot scale ArialMT fonts; size 3 activates the glcdfont6×8 renderer
      // (drawV3ClockString) which draws at textSize=3 and maps pixels through SCALE_X/Y.
      _use_v3_clock_font = true;
      break;
    default:
#ifdef CYRILLIC_SUPPORT
      display.setFont(ArialMT_Plain_16_RU);
#else
      display.setFont(ArialMT_Plain_16);
#endif
  }
}

void ST7789Display::setColor(ColorVal c) {
  _color = c;
  display.setColor((OLEDDISPLAY_COLOR)_color);
  display.setRGB(_color == OLEDDISPLAY_COLOR::WHITE ? ST77XX_WHITE : ST77XX_BLACK);
}

void ST7789Display::setCursor(int x, int y) {
  _logical_x = x;
  _logical_y = y;
  _x = x*SCALE_X + X_OFFSET;
  _y = y*SCALE_Y + Y_OFFSET;
}

void ST7789Display::print(const char* str) {
  if (_use_v3_clock_font) {
    drawV3ClockString(_logical_x, _logical_y, str);
  } else if (_text_scale > 1) {
    drawScaledString(_x, _y, str);
  } else {
    display.drawString(_x, _y, str);
  }
}

void ST7789Display::printWordWrap(const char* str, int max_width) {
  display.drawStringMaxWidth(_x, _y, max_width*SCALE_X, str);
}

void ST7789Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x*SCALE_X + X_OFFSET, y*SCALE_Y + Y_OFFSET, w*SCALE_X, h*SCALE_Y);
}

void ST7789Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x*SCALE_X + X_OFFSET, y*SCALE_Y + Y_OFFSET, w*SCALE_X, h*SCALE_Y);
}

void ST7789Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  // Each virtual pixel becomes a SCALE×SCALE block. Block boundaries are computed
  // per-pixel (not as startX + bx*SCALE) to avoid cumulative rounding error across
  // the bitmap — keeps icon edges sharp and proportions square.
  uint16_t startX = x * SCALE_X + X_OFFSET;
  uint16_t startY = y * SCALE_Y + Y_OFFSET;
  uint16_t widthInBytes = (w + 7) / 8;

  for (uint16_t by = 0; by < h; by++) {
    int y1 = startY + (int)(by * SCALE_Y);
    int y2 = startY + (int)((by + 1) * SCALE_Y);
    int block_h = y2 - y1;
    for (uint16_t bx = 0; bx < w; bx++) {
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      if (pgm_read_byte(bits + byteOffset) & (0x80 >> (bx & 7))) {
        int x1 = startX + (int)(bx * SCALE_X);
        int x2 = startX + (int)((bx + 1) * SCALE_X);
        display.fillRect(x1, y1, x2 - x1, block_h);
      }
    }
  }
}

uint16_t ST7789Display::getTextWidth(const char* str) {
  if (_use_v3_clock_font) return getV3ClockTextWidth(str);
  uint16_t phys_width = (_text_scale > 1) ? getScaledTextWidth(str) : display.getStringWidth(str);
  return phys_width / SCALE_X;
}

void ST7789Display::drawTextCentered(int mid_x, int y, const char* str) {
  if (_use_v3_clock_font) {
    int w = getV3ClockTextWidth(str);
    drawV3ClockString(mid_x - w / 2, y, str);
    return;
  }
  int phys_mid_x = mid_x * SCALE_X + X_OFFSET;
  int phys_y = y * SCALE_Y + Y_OFFSET;
  int phys_w = (_text_scale > 1) ? getScaledTextWidth(str) : display.getStringWidth(str);
  if (_text_scale > 1) {
    drawScaledString(phys_mid_x - phys_w / 2, phys_y, str);
  } else {
    display.drawString(phys_mid_x - phys_w / 2, phys_y, str);
  }
}

void ST7789Display::endFrame() {
  display.display();
}

// Returns physical-space width of str rendered at _text_scale, used for manual centering
// before calling drawScaledString (which takes physical x,y).
uint16_t ST7789Display::getScaledTextWidth(const char* str) const {
  const uint8_t* fontData = display.getFontData();
  if (fontData == nullptr || str == nullptr) return 0;

  uint8_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
  uint8_t charCount = pgm_read_byte(fontData + CHAR_NUM_POS);
  uint16_t width = 0;

  for (const char* p = str; *p; p++) {
    uint8_t code = (uint8_t)*p;
    if (code >= firstChar && code < firstChar + charCount) {
      width += pgm_read_byte(fontData + JUMPTABLE_START +
                             (code - firstChar) * JUMPTABLE_BYTES +
                             JUMPTABLE_WIDTH) * _text_scale;
    }
  }
  return width;
}

// Renders str at physical (x, y) by drawing each font bit as a _text_scale×_text_scale block.
// Needed because OLEDDisplay::drawString writes to a 1-bpp framebuffer and cannot upscale.
// x, y here are physical coordinates (already transformed by SCALE + OFFSET).
void ST7789Display::drawScaledString(int x, int y, const char* str) {
  const uint8_t* fontData = display.getFontData();
  if (fontData == nullptr || str == nullptr) return;

  uint8_t textHeight = pgm_read_byte(fontData + HEIGHT_POS);
  uint8_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
  uint8_t charCount = pgm_read_byte(fontData + CHAR_NUM_POS);
  uint16_t jumpTableSize = charCount * JUMPTABLE_BYTES;
  uint8_t rasterHeight = 1 + ((textHeight - 1) >> 3);
  int cursorX = 0;

  for (const char* p = str; *p; p++) {
    uint8_t code = (uint8_t)*p;
    if (code < firstChar || code >= firstChar + charCount) continue;

    uint8_t charCode = code - firstChar;
    uint16_t jump = ((uint16_t)pgm_read_byte(fontData + JUMPTABLE_START +
                                             charCode * JUMPTABLE_BYTES) << 8) |
                    pgm_read_byte(fontData + JUMPTABLE_START +
                                  charCode * JUMPTABLE_BYTES + JUMPTABLE_LSB);
    uint8_t byteSize = pgm_read_byte(fontData + JUMPTABLE_START +
                                     charCode * JUMPTABLE_BYTES + JUMPTABLE_SIZE);
    uint8_t charWidth = pgm_read_byte(fontData + JUMPTABLE_START +
                                      charCode * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);

    if (jump != 0xFFFF) {
      uint16_t charData = JUMPTABLE_START + jumpTableSize + jump;
      for (uint8_t col = 0; col < charWidth; col++) {
        for (uint8_t row = 0; row < textHeight; row++) {
          uint16_t byteIndex = col * rasterHeight + (row >> 3);
          if (byteIndex >= byteSize) continue;
          uint8_t bits = pgm_read_byte(fontData + charData + byteIndex);
          if (bits & (1 << (row & 7))) {
            display.fillRect(x + cursorX + col * _text_scale,
                             y + row * _text_scale,
                             _text_scale,
                             _text_scale);
          }
        }
      }
    }
    cursorX += charWidth * _text_scale;
  }
}

// Returns virtual-space width so drawTextCentered can compute virtual x offset.
// 6 columns/char × textSize=3 virtual px/column.
uint16_t ST7789Display::getV3ClockTextWidth(const char* str) const {
  return str == nullptr ? 0 : strlen(str) * 6 * 3;
}

// Renders str using the glcdfont 6×8 bitmap at textSize=3, mapped to physical pixels
// through the same SCALE_X/SCALE_Y/Y_OFFSET as all other drawing primitives.
// x, y are VIRTUAL coordinates (same space as setCursor).
// Block boundaries are recomputed per sub-pixel to eliminate cumulative rounding drift.
void ST7789Display::drawV3ClockString(int x, int y, const char* str) {
  if (str == nullptr) return;

  const int textSize = 3;
  int cursorX = x;

  for (const char* p = str; *p; p++) {
    uint8_t code = (uint8_t)*p;
    if (code < 32) code = 32;
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 6; col++) {
        uint16_t bit = row * 6 + col;
        uint8_t packed = pgm_read_byte(glcdfont6x8Bitmaps + (code - 32) * 6 + (bit >> 3));
        if ((packed & (0x80 >> (bit & 7))) == 0) continue;

        for (int sy = 0; sy < textSize; sy++) {
          for (int sx = 0; sx < textSize; sx++) {
            int vx = cursorX + col * textSize + sx;
            int vy = y + row * textSize + sy;
            int x1 = (int)(vx * SCALE_X) + X_OFFSET;
            int x2 = (int)((vx + 1) * SCALE_X) + X_OFFSET;
            int y1 = (int)(vy * SCALE_Y) + Y_OFFSET;
            int y2 = (int)((vy + 1) * SCALE_Y) + Y_OFFSET;
            int blockW = x2 - x1;
            int blockH = y2 - y1;
            if (blockW < 1) blockW = 1;
            if (blockH < 1) blockH = 1;
            display.fillRect(x1, y1, blockW, blockH);
          }
        }
      }
    }
    cursorX += 6 * textSize;
  }
}

#endif
