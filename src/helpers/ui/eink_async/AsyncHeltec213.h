#pragma once

#include <GxEPD2_BW.h>
#include "AsyncEinkCommon.h"
#include "HeltecE213Luts.h"

// Неблокирующий драйвер 2.13" панелей Heltec: Vision Master E213 и Wireless
// Paper. На этих платах встречаются две панели с разными контроллерами, и
// какая стоит — по маркировке платы не понять, поэтому она определяется при
// старте, как это делает синхронный драйвер (E213Display::detectEInk):
//
//   LCMEN2R13EFC1 — контроллер UC81xx (Fitipower), BUSY активен по LOW.
//                   Vision Master E213, Wireless Paper V1.1.
//   E0213A367     — контроллер семейства SSD (Solomon), BUSY активен по HIGH.
//                   Vision Master E213 V1.1, Wireless Paper V1.1.1 / V1.2.
//
// Последовательности команд перенесены из heltec-eink-modules — драйвера,
// который на этих платах рисует: Displays/LCMEN2R13EFC1 и Displays/E0213A367.
// Классы GxEPD2 здесь не подходят ни для одной из панелей: у UC81xx другая
// система команд, а E0213A367 задаёт окно и курсор по Y одним байтом.
//
// Wireless Paper V1 (DEPG0213BNS800) этим путём не поддерживается — его не
// различает и синхронный драйвер.
class AsyncHeltec213 {
public:
  using PollResult = AsyncEinkPollResult;

  static const uint16_t WIDTH = 128;
  static const uint16_t WIDTH_VISIBLE = 122;
  static const uint16_t HEIGHT = 250;
  static const bool hasFastPartialUpdate = true;

  // Номинальные времена: нижняя граница опроса BUSY не нужна (линия у обеих
  // панелей надёжна при поданном питании), это только основа для таймаута.
  static const uint16_t full_refresh_time = 4000;
  static const uint16_t partial_refresh_time = 700;
  static const uint16_t power_off_time = 200;

  AsyncHeltec213(int16_t cs, int16_t dc, int16_t rst, int16_t busy)
    : _cs(cs), _dc(dc), _rst(rst), _busy(busy) {}

  void selectSPI(SPIClass& spi, SPISettings settings) {
    _spi = &spi;
    _spi_settings = settings;
  }

  // Вызывается из AsyncEinkCanvas::init(). Шина уже поднята GxEPDDisplay.
  void init(uint32_t, bool, uint16_t reset_duration, bool) {
    _reset_duration = reset_duration < 10 ? 10 : reset_duration;
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    pinMode(_dc, OUTPUT);
    digitalWrite(_dc, HIGH);
    pinMode(_busy, INPUT);
    detectPanel();
    _init_done = false;
    _config = Config::None;
    _operation = Operation::None;
  }

  // Канва пишет только изменившуюся полосу строк. UC81xx окна не
  // поддерживает — ему всегда нужен кадр целиком.
  bool fullFrameWrites() const { return _uc; }

  // Новый кадр. Для частичного обновления — только текущий буфер: старый
  // хранит прошлый кадр, по разнице с ним панель и рисует.
  void writeImage(const uint8_t* bitmap, int16_t, int16_t y, int16_t, int16_t h) {
    ensureInit();
    if (_uc) writeFrame(0x13, bitmap);
    else writeRows(0x24, bitmap, y, h);
  }

  // Полное обновление: оба буфера получают новый кадр.
  void writeImageForFullRefresh(const uint8_t* bitmap, int16_t, int16_t y, int16_t, int16_t h) {
    ensureInit();
    if (_uc) {
      writeFrame(0x10, bitmap);
      writeFrame(0x13, bitmap);
    } else {
      writeRows(0x26, bitmap, y, h);
      writeRows(0x24, bitmap, y, h);
    }
  }

  // После обновления: старый буфер приравнивается к показанному кадру,
  // чтобы следующий частичный кадр считал разницу от него.
  void writeImageAgain(const uint8_t* bitmap, int16_t, int16_t y, int16_t, int16_t h) {
    ensureInit();
    if (_uc) writeFrame(0x10, bitmap);
    else writeRows(0x26, bitmap, y, h);
  }

  bool asyncBusy() const { return _operation != Operation::None; }

  bool startRefreshAsync(bool partial) {
    if (_operation != Operation::None) return false;
    ensureInit();
    applyConfig(partial ? Config::Partial : Config::Full);
#if defined(EINK_ASYNC_LOG)
    Serial.printf("[EINK] refresh start partial=%d panel=%s\n", partial ? 1 : 0, _uc ? "UC81xx" : "SSD");
#endif
    _partial_refresh = partial;
    if (_uc) {
      // UC81xx: питание (0x04), по его готовности — обновление (0x12).
      command(0x04);
      start(Operation::PowerOn, 200);
    } else {
      // E0213A367: питание и выключение входят в саму последовательность,
      // отдельного шага выключения не нужно.
      command(0x22);
      data(partial ? 0xFF : 0xF7);
      command(0x20);
      start(Operation::Refresh, partial ? partial_refresh_time : full_refresh_time);
    }
    return true;
  }

  bool startPowerOffAsync() {
    if (_operation != Operation::None) return false;
    if (!_uc || !_power_is_on) return false;
    command(0x02);
    start(Operation::PowerOff, power_off_time);
    return true;
  }

  PollResult pollAsync() {
    if (_operation == Operation::None) return PollResult::Idle;
    const uint32_t elapsed = millis() - _started_at;
    if (elapsed < ASYNC_EINK_BUSY_ASSERT_MS || panelBusy()) {
      if (elapsed <= _timeout_ms) return PollResult::Busy;
#if defined(EINK_ASYNC_LOG)
      Serial.printf("[EINK] op %d TIMEOUT after %lu ms\n", int(_operation), (unsigned long)elapsed);
#endif
      _operation = Operation::None;
      _init_done = false;   // контроллер в неизвестном состоянии — заново с аппаратного сброса
      return PollResult::TimedOut;
    }
    if (_operation == Operation::PowerOn) {
      _power_is_on = true;
      command(0x12);
      start(Operation::Refresh, _partial_refresh ? partial_refresh_time : full_refresh_time);
      return PollResult::Busy;
    }
#if defined(EINK_ASYNC_LOG)
    Serial.printf("[EINK] op %d done after %lu ms\n", int(_operation), (unsigned long)elapsed);
#endif
    if (_operation == Operation::PowerOff) _power_is_on = false;
    _operation = Operation::None;
    return PollResult::Complete;
  }

  // Блокирующие варианты — для загрузки и выключения (AsyncEinkCanvas::display).
  void refresh(bool partial) {
    if (!startRefreshAsync(partial)) return;
    waitOperation();
  }

  void powerOff() {
    if (startPowerOffAsync()) waitOperation();
  }

  void hibernate() {
    powerOff();
    // SSD: глубокий сон с сохранением RAM (режим 1), выход — аппаратным
    // сбросом, который делает следующий ensureInit(). UC81xx в глубоком сне
    // теряет буферы, а без старого кадра частичное обновление рисует мусор,
    // поэтому ему достаточно выключенного питания панели.
    if (!_uc && _init_done) {
      command(0x10);
      data(0x01);
      _init_done = false;
    }
  }

private:
  enum class Operation : uint8_t { None, PowerOn, Refresh, PowerOff };
  enum class Config : uint8_t { None, Full, Partial };

  int16_t _cs, _dc, _rst, _busy;
  SPIClass* _spi = &SPI;
  SPISettings _spi_settings = SPISettings(4000000, MSBFIRST, SPI_MODE0);
  uint16_t _reset_duration = 10;
  bool _uc = false;
  bool _init_done = false;
  bool _power_is_on = false;
  bool _partial_refresh = false;
  Config _config = Config::None;
  Operation _operation = Operation::None;
  uint32_t _started_at = 0;
  uint32_t _timeout_ms = 0;

  // Как E213Display::detectEInk(): при удержании RST контроллер «занят»,
  // и уровень BUSY в этот момент выдаёт производителя.
  void detectPanel() {
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(10);
    _uc = (digitalRead(_busy) == LOW);
    digitalWrite(_rst, HIGH);
    delay(10);
#if defined(EINK_ASYNC_LOG)
    Serial.printf("[EINK] panel detected: %s\n", _uc ? "LCMEN2R13EFC1 (UC81xx)" : "E0213A367 (SSD)");
#endif
  }

  bool panelBusy() const {
    return digitalRead(_busy) == (_uc ? LOW : HIGH);
  }

  void waitBusyBlocking(uint32_t max_ms) {
    delay(1);
    const uint32_t t0 = millis();
    while (panelBusy() && millis() - t0 < max_ms) delay(1);
  }

  void waitOperation() {
    while (pollAsync() == PollResult::Busy) delay(1);
  }

  void start(Operation op, uint32_t nominal_ms) {
    _operation = op;
    _started_at = millis();
    _timeout_ms = nominal_ms + ASYNC_EINK_TIMEOUT_SLACK_MS;
  }

  // Аппаратный сброс и, для SSD, программный. Конфигурация режима после
  // сброса потеряна — её заново выставит applyConfig().
  void ensureInit() {
    if (_init_done) return;
    digitalWrite(_rst, LOW);
    delay(_reset_duration);
    digitalWrite(_rst, HIGH);
    delay(10);
    waitBusyBlocking(1000);
    if (!_uc) {
      command(0x12);   // SWRESET
      waitBusyBlocking(1000);
    }
    _power_is_on = false;
    _config = Config::None;
    _init_done = true;
  }

  void applyConfig(Config want) {
    if (_config == want) return;
    if (_uc) {
      if (want == Config::Partial) {
        command(0x00); data(0xFF);   // panel setting: таблицы из регистров
        command(0x50); data(0xD7);   // VCOM и интервал данных
        writeLut(0x20, LUT_PARTIAL_VCOM_DC, sizeof(LUT_PARTIAL_VCOM_DC));
        writeLut(0x21, LUT_PARTIAL_WW, sizeof(LUT_PARTIAL_WW));
        writeLut(0x22, LUT_PARTIAL_BW, sizeof(LUT_PARTIAL_BW));
        writeLut(0x23, LUT_PARTIAL_WB, sizeof(LUT_PARTIAL_WB));
        writeLut(0x24, LUT_PARTIAL_BB, sizeof(LUT_PARTIAL_BB));
      } else {
        command(0x00); data(0xDF);   // panel setting: таблицы из OTP
        command(0x50); data(0x97);
      }
    } else {
      // Какие волновые формы OTP считать быстрыми (одинаково для обоих режимов).
      command(0x37);
      data(0x00); data(0x80); data(0x03); data(0x0E);
      command(0x3C);                 // граница
      data(want == Config::Partial ? 0x80 : 0x01);
      waitBusyBlocking(1000);
    }
    _config = want;
  }

  void writeLut(uint8_t cmd, const uint8_t* lut, size_t n) {
    command(cmd);
    _spi->beginTransaction(_spi_settings);
    digitalWrite(_cs, LOW);
    for (size_t i = 0; i < n; i++) _spi->transfer(lut[i]);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
  }

  // UC81xx: окна нет, буфер пишется целиком. Канва при fullFrameWrites()
  // всегда передаёт кадр с первой строки.
  void writeFrame(uint8_t cmd, const uint8_t* frame) {
    command(cmd);
    sendBytes(frame, size_t(WIDTH / 8) * HEIGHT);
  }

  // E0213A367: окно по строкам; Y у этого контроллера — один байт.
  void writeRows(uint8_t cmd, const uint8_t* rows, int16_t y, int16_t h) {
    if (y < 0 || h <= 0 || y + h > HEIGHT) return;
    command(0x11); data(0x03);                         // X+, Y+
    command(0x44); data(0x00); data(WIDTH / 8 - 1);    // X от и до (байты)
    command(0x45); data(uint8_t(y)); data(uint8_t(y + h - 1));
    command(0x4E); data(0x00);
    command(0x4F); data(uint8_t(y));
    command(cmd);
    sendBytes(rows, size_t(WIDTH / 8) * h);
  }

  void command(uint8_t c) {
    _spi->beginTransaction(_spi_settings);
    digitalWrite(_dc, LOW);
    digitalWrite(_cs, LOW);
    _spi->transfer(c);
    digitalWrite(_cs, HIGH);
    digitalWrite(_dc, HIGH);
    _spi->endTransaction();
  }

  void data(uint8_t d) {
    _spi->beginTransaction(_spi_settings);
    digitalWrite(_cs, LOW);
    _spi->transfer(d);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
  }

  void sendBytes(const uint8_t* p, size_t n) {
    _spi->beginTransaction(_spi_settings);
    digitalWrite(_cs, LOW);
    for (size_t i = 0; i < n; i++) _spi->transfer(p[i]);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
    delay(1);   // уступить планировщику после длинной передачи
  }
};
