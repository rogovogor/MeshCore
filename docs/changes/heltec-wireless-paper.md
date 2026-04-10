# Heltec Wireless Paper — корректное измерение напряжения батареи

## Вектор изменений

Выделение специфичного для Wireless Paper измерения батареи в отдельный подкласс вместо использования унаследованной реализации HeltecV3Board.

---

## Новый файл: `variants/heltec_wireless_paper/WirelessPaperBoard.h`

```cpp
class WirelessPaperBoard : public HeltecV3Board {
public:
  uint16_t getBattMilliVolts() override {
    // аппаратно: Q3 (AO3401) + делитель R21=10k/R26=10k → множитель 2
    uint32_t mv = 0;
    for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_VBAT_READ);
    mv /= 8;
    return mv * 2;
  }
  const char* getManufacturerName() const override { return "Heltec Wireless Paper"; }
};
```

Ключевые отличия от HeltecV3Board:
- Используется `analogReadMilliVolts()` вместо `analogRead()` — даёт результат сразу в мВ, без ручной конвертации АЦП-кодов.
- Множитель `× 2` соответствует делителю напряжения 10k/10k по схеме Wireless Paper V0.4.
- Усреднение по 8 замерам для снижения шума АЦП.

---

## Изменения в существующих файлах

### `variants/heltec_wireless_paper/target.h`
- `#include <../heltec_v3/HeltecV3Board.h>` → `#include "WirelessPaperBoard.h"`
- `extern HeltecV3Board board` → `extern WirelessPaperBoard board`

### `variants/heltec_wireless_paper/target.cpp`
- `HeltecV3Board board` → `WirelessPaperBoard board`

### `variants/heltec_wireless_paper/platformio.ini`
- Добавлен `-D CYRILLIC_SUPPORT=1` (см. [cyrillic-support.md](cyrillic-support.md)).
