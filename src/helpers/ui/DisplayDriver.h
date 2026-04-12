#pragma once

#include <stdint.h>
#include <string.h>

class DisplayDriver {
  int _w, _h;
protected:
  DisplayDriver(int w, int h) { _w = w; _h = h; }
#ifdef CYRILLIC_SUPPORT
  uint8_t _font_size = 1;     // current text size; used for baseline offset and word-wrap metrics
  int _cursor_y_raw = 0;      // logical y before GFXfont baseline shift; tracks line position in printWordWrap
#endif
public:
  enum Color { DARK=0, LIGHT, RED, GREEN, BLUE, YELLOW, ORANGE }; // on b/w screen, colors will be !=0 synonym of light

  int width() const { return _w; }
  int height() const { return _h; }

  virtual bool isEink() const { return false; }
  virtual bool isOn() = 0;
  virtual void turnOn() = 0;
  virtual void turnOff() = 0;
  virtual void clear() = 0;
  virtual void startFrame(Color bkg = DARK) = 0;
  virtual void setTextSize(int sz) = 0;
  virtual void setColor(Color c) = 0;
  virtual void setCursor(int x, int y) = 0;
  virtual void print(const char* str) = 0;
  virtual void printWordWrap(const char* str, int max_width) {
#ifdef CYRILLIC_SUPPORT
    // glcdfont6x8 is monospace: all glyphs 6 px wide, 8 px tall (scaled by _font_size)
    const int char_w = 6 * _font_size;
    const int line_h = 8 * _font_size;
    const int max_chars = max_width / char_w;

    int len = (int)strlen(str);
    int pos = 0;
    char line_buf[64];   // one line: up to ~41 chars + NUL at size 1

    while (pos < len && _cursor_y_raw + line_h <= height()) {
      if (len - pos <= max_chars) {
        print(str + pos);
        break;
      }
      // prefer breaking on a space to avoid mid-word splits
      int break_at = pos + max_chars;
      for (int i = pos + max_chars; i > pos; i--) {
        if (str[i] == ' ') { break_at = i; break; }
      }
      int seg_len = break_at - pos;
      if (seg_len > (int)sizeof(line_buf) - 1) seg_len = (int)sizeof(line_buf) - 1;
      memcpy(line_buf, str + pos, seg_len);
      line_buf[seg_len] = 0;
      print(line_buf);

      pos = break_at + (str[break_at] == ' ' ? 1 : 0);
      _cursor_y_raw += line_h;
      if (_cursor_y_raw + line_h <= height()) setCursor(0, _cursor_y_raw);
    }
#else
    print(str);
#endif
  }
  virtual void fillRect(int x, int y, int w, int h) = 0;
  virtual void drawRect(int x, int y, int w, int h) = 0;
  virtual void drawXbm(int x, int y, const uint8_t* bits, int w, int h) = 0;
  virtual uint16_t getTextWidth(const char* str) = 0;
  virtual void drawTextCentered(int mid_x, int y, const char* str) {   // helper method (override to optimise)
    int w = getTextWidth(str);
    setCursor(mid_x - w/2, y);
    print(str);
  }
  virtual void drawTextRightAlign(int x_anch, int y, const char* str) {
    int w = getTextWidth(str);
    setCursor(x_anch - w, y);
    print(str);
  }
  virtual void drawTextLeftAlign(int x_anch, int y, const char* str) {
    setCursor(x_anch, y);
    print(str);
  }
  
  // Convert UTF-8 string to a single-byte encoding suitable for the active font.
  // With CYRILLIC_SUPPORT: maps Cyrillic UTF-8 sequences to CP1251 code points used
  // by glcdfont6x8; unmapped non-ASCII bytes become 0xAE (CP1251 smile ®).
  // Without CYRILLIC_SUPPORT: replaces every non-ASCII byte with 0xDB (CP437 block █).
  virtual void translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) {
    size_t j = 0;
#ifdef CYRILLIC_SUPPORT
    char lead = 0;
    char cc = 0;
#endif
    for (size_t i = 0; src[i] != 0 && j < dest_size - 1; i++) {
      unsigned char c = (unsigned char)src[i];
      if (c >= 32 && c <= 126) {
#ifdef CYRILLIC_SUPPORT
        lead = 0;
        dest[j++] = c;
      } else if (c == 0xC2 || c == 0xC3 || c == 0xD0 || c == 0xD1 || c == 0xD2) {
        // UTF-8 two-byte Cyrillic lead byte — consume continuation byte
        lead = (char)c;
        cc = 0;
        c = (unsigned char)src[++i];
        if (c != 0) {
          switch (lead) {
            case 0xC3: cc = (char)(c | 0xC0); break;
            case 0xD0:
              if      (c == 129) cc = 168;           // Ё
              else if (c == 132) cc = 170;            // Є
              else if (c == 134) cc = 178;            // І
              else if (c == 135) cc = 175;            // Ї
              else if (c > 143 && c < 192) cc = (char)(c + 48);  // А-Я
              break;
            case 0xD1:
              if      (c == 145) cc = 184;            // ё
              else if (c == 148) cc = 186;            // є
              else if (c == 150) cc = 179;            // і
              else if (c == 151) cc = 191;            // ї
              else if (c > 127 && c < 144) cc = (char)(c + 112); // а-я
              break;
            case 0xD2:
              if      (c == 144) cc = 165;            // Ґ
              else if (c == 145) cc = 180;            // ґ
              break;
          }
          dest[j++] = (cc != 0) ? cc : '\xAE';  // fallback: ® smile in CP1251
        }
      } else if (c >= 0x80) {
        lead = 0;
        dest[j++] = '\xAE';  // non-Cyrillic non-ASCII: smile placeholder
#else
        dest[j++] = c;  // ASCII printable
      } else if (c >= 0x80) {
        dest[j++] = '\xDB';  // CP437 full block █
#endif
        while (src[i+1] && (src[i+1] & 0xC0) == 0x80)
          i++;  // skip UTF-8 continuation bytes
      }
    }
    dest[j] = 0;
  }
  
  // draw text with ellipsis if it exceeds max_width
  virtual void drawTextEllipsized(int x, int y, int max_width, const char* str) {
    char temp_str[256];  // reasonable buffer size
    size_t len = strlen(str);
    if (len >= sizeof(temp_str)) len = sizeof(temp_str) - 1;
    memcpy(temp_str, str, len);
    temp_str[len] = 0;
    
    if (getTextWidth(temp_str) <= max_width) {
      setCursor(x, y);
      print(temp_str);
      return;
    }
    
    // for variable-width fonts (GxEPD), add space after ellipsis
    // for fixed-width fonts (OLED), keep tight spacing to save precious characters
    const char* ellipsis;
    // use a simple heuristic: if 'i' and 'l' have different widths, it's variable-width
    int i_width = getTextWidth("i");
    int l_width = getTextWidth("l");
    if (i_width != l_width) {
      ellipsis = "... ";  // variable-width fonts: add space
    } else {
      ellipsis = "...";   // fixed-width fonts: no space
    }
    
    int ellipsis_width = getTextWidth(ellipsis);
    int str_len = strlen(temp_str);
    
    while (str_len > 0 && getTextWidth(temp_str) > max_width - ellipsis_width) {
      temp_str[--str_len] = 0;
    }
    strcat(temp_str, ellipsis);
    
    setCursor(x, y);
    print(temp_str);
  }
  
  virtual void endFrame() = 0;
};
