#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>

enum class AsyncEinkPollResult : uint8_t { Idle, Busy, Complete, TimedOut };

// Timing shared by every panel adapter. These live here, not in the adapters,
// because the adapters are otherwise near-copies of each other and a value
// tuned in one would silently not be tuned in the other.

// How long after issuing the refresh command the BUSY line is not yet
// trustworthy. GxEPD2 covers this with a delay(1) before its first read; the
// async path cannot block, so it declines to believe a low BUSY until this
// much has passed. Reading too early sees "not busy" for a refresh that has
// not started and tears down the panel mid-waveform.
static const uint32_t ASYNC_EINK_BUSY_ASSERT_MS = 2;

// Slack over the nominal refresh time before a refresh is declared stuck.
// Matches GxEPD2's own 10 s _busy_timeout rather than undercutting it: e-ink
// slows down markedly in the cold, and a 2 s margin turns an ordinary winter
// refresh into a timeout.
static const uint32_t ASYNC_EINK_TIMEOUT_SLACK_MS = 10000;

// Small, local full-frame canvas used by the asynchronous e-ink path.
// GxEPD2_BW keeps its framebuffer private, so it cannot split display() into
// "start refresh" and "finish refresh" without modifying the dependency.
// Drawing still uses Adafruit_GFX; only buffer ownership lives here.
template<typename Panel>
class AsyncEinkCanvas : public Adafruit_GFX {
public:
  using PollResult = AsyncEinkPollResult;

  Panel epd2;

  explicit AsyncEinkCanvas(Panel panel)
    : Adafruit_GFX(Panel::WIDTH_VISIBLE, Panel::HEIGHT), epd2(panel) {
    fillScreen(GxEPD_WHITE);
  }

  void init(uint32_t bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode) {
    epd2.init(bitrate, initial, reset_duration, pulldown_rst_mode);
  }

  // GxEPDDisplay always renders a complete frame. Keep the method for source
  // compatibility with GxEPD2_BW, but no partial canvas bookkeeping is needed.
  void setPartialWindow(uint16_t, uint16_t, uint16_t, uint16_t) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) return;
    switch (getRotation()) {
      case 1:
        swap(x, y);
        x = WIDTH - x - 1;
        break;
      case 2:
        x = WIDTH - x - 1;
        y = HEIGHT - y - 1;
        break;
      case 3:
        swap(x, y);
        y = HEIGHT - y - 1;
        break;
    }
    if ((x < 0) || (x >= int16_t(Panel::WIDTH)) || (y < 0) || (y >= int16_t(Panel::HEIGHT))) return;
    const uint32_t index = uint32_t(x / 8) + uint32_t(y) * (Panel::WIDTH / 8);
    const uint8_t mask = uint8_t(1U << (7 - x % 8));
    if (color) _buffer[index] |= mask;
    else _buffer[index] &= uint8_t(~mask);
  }

  void fillScreen(uint16_t color) {
    memset(_buffer, color == GxEPD_BLACK ? 0x00 : 0xFF, sizeof(_buffer));
  }

  // Blocking compatibility path used only for boot initialization and clean
  // shutdown. Normal UI frames use startDisplay()/poll()/finishDisplay().
  void display(bool partial) {
    writeForRefresh(partial);
    epd2.refresh(partial);
    finishDisplay();
    if (!partial) epd2.powerOff();
  }

  void powerOff() { epd2.powerOff(); }
  void hibernate() { epd2.hibernate(); }

  bool startDisplay(bool partial) {
    if (epd2.asyncBusy()) return false;
    writeForRefresh(partial);
    return epd2.startRefreshAsync(partial);
  }

  PollResult poll() { return epd2.pollAsync(); }

  // Fast differential panels need the just-displayed image copied into both
  // controller RAM planes. The framebuffer must remain unchanged until this
  // method completes.
  void finishDisplay() {
    if (Panel::hasFastPartialUpdate) {
      // Same band writeForRefresh() sent: the two controller RAM planes only
      // need to agree where the image actually changed.
      epd2.writeImageAgain(_buffer + size_t(_dirty_first) * BYTES_PER_ROW,
                           0, _dirty_first, Panel::WIDTH, _dirty_rows);
    }
  }

  bool startPowerOff() { return epd2.startPowerOffAsync(); }

private:
  static const uint16_t BYTES_PER_ROW = Panel::WIDTH / 8;
  uint8_t _buffer[BYTES_PER_ROW * Panel::HEIGHT];

  // One checksum per panel row, from the frame last sent. A partial refresh
  // only drives the rows it addresses, so sending just the band that changed
  // is the difference between redrawing the whole panel and redrawing a
  // keyboard row. Measured in the emulator: a cursor step changes 8-17% of
  // rows, everything else was being refreshed for nothing.
  //
  // Checksums rather than a second framebuffer: 2 bytes a row against
  // WIDTH/8, which on the 2.9" is 592 bytes instead of 4736.
  uint16_t _row_sum[Panel::HEIGHT] = {0};
  bool     _row_sum_valid = false;
  uint16_t _dirty_first = 0;
  uint16_t _dirty_rows  = Panel::HEIGHT;

  static uint16_t rowSum(const uint8_t* row) {
    uint16_t s = 0;
    for (uint16_t i = 0; i < BYTES_PER_ROW; i++) s = uint16_t(s * 31u + row[i]);
    return s;
  }

  // Narrow the write to the rows that differ from the frame on the panel, and
  // remember the band so finishDisplay() syncs exactly the same one.
  void markDirtyBand() {
    if (!_row_sum_valid) {                 // first frame: nothing to compare to
      _dirty_first = 0;
      _dirty_rows  = Panel::HEIGHT;
      return;
    }
    int first = -1, last = -1;
    for (uint16_t y = 0; y < Panel::HEIGHT; y++) {
      if (rowSum(_buffer + size_t(y) * BYTES_PER_ROW) != _row_sum[y]) {
        if (first < 0) first = y;
        last = y;
      }
    }
    if (first < 0) {                       // identical frame; caller normally
      _dirty_first = 0;                    // filters these out by CRC first
      _dirty_rows  = 1;
      return;
    }
    _dirty_first = uint16_t(first);
    _dirty_rows  = uint16_t(last - first + 1);
  }

  void rememberRows() {
    for (uint16_t y = 0; y < Panel::HEIGHT; y++)
      _row_sum[y] = rowSum(_buffer + size_t(y) * BYTES_PER_ROW);
    _row_sum_valid = true;
  }

  // Panels whose controller has no RAM window (UC81xx) take the whole frame
  // on every write. Optional: a panel without fullFrameWrites() gets bands.
  template<typename P>
  static auto fullFrameImpl(const P& p, int) -> decltype(p.fullFrameWrites()) { return p.fullFrameWrites(); }
  template<typename P>
  static bool fullFrameImpl(const P&, long) { return false; }

  void writeForRefresh(bool partial) {
    if (partial) {
      if (fullFrameImpl(epd2, 0)) {
        _dirty_first = 0;
        _dirty_rows  = Panel::HEIGHT;
      } else {
        markDirtyBand();
      }
      epd2.writeImage(_buffer + size_t(_dirty_first) * BYTES_PER_ROW,
                      0, _dirty_first, Panel::WIDTH, _dirty_rows);
    } else {
      // A full refresh rewrites the whole panel, so the band is the panel.
      _dirty_first = 0;
      _dirty_rows  = Panel::HEIGHT;
      epd2.writeImageForFullRefresh(_buffer, 0, 0, Panel::WIDTH, Panel::HEIGHT);
    }
    rememberRows();
  }

  template<typename T>
  static void swap(T& a, T& b) {
    T tmp = a;
    a = b;
    b = tmp;
  }
};
