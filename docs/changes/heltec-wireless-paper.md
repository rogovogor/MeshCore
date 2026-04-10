# Heltec Wireless Paper — корректное измерение напряжения батареи

## Вектор изменений

Выделение специфичного для Wireless Paper измерения батареи в отдельный подкласс вместо использования унаследованной реализации HeltecV3Board. Три последовательных исправления по результатам тестирования на железе.

---

## Диагностика (результаты тестов на железе)

| Факт (мультиметр) | До фикса | После фикса |
|---|---|---|
| 3.9V | 4.27V / 100% (ошибка +9.5%) | ~3.9V / 75% |
| 4.2V (полный) | >4.2V → 100% clamped | ~4.2V / 100% |
| 3.3V (порог) | нет shutdown | → программный shutdown |

---

## Исправление 1: неверный множитель (`22d26dd5`)

До исправления Wireless Paper наследовал `getBattMilliVolts()` от `HeltecV3Board`,
который использует коэффициент `5.42` — подобранный под другую схему делителя.

Введён новый класс `WirelessPaperBoard`. Делитель по схеме Wireless Paper V0.4:
Q3 (AO3401) + R21=10k / R26=10k → теоретический множитель = 2.

**Новые файлы/изменения:**
- `variants/heltec_wireless_paper/WirelessPaperBoard.h` — новый класс
- `variants/heltec_wireless_paper/target.h` / `target.cpp` — используют `WirelessPaperBoard`

---

## Исправление 2: некалиброванный ADC (`b787accf`)

`analogRead()` возвращает сырые АЦП-коды без учёта нелинейности ESP32-S3.
Заменён на `analogReadMilliVolts()` — использует eFuse-калибровку чипа.

```cpp
// было:
analogReadResolution(10);
raw += analogRead(PIN_VBAT_READ);
return (2.0 * (3.3 / 1024.0) * raw) * 1000;

// стало:
mv += analogReadMilliVolts(PIN_VBAT_READ);
return mv * 2;
```

---

## Исправление 3: остаточная погрешность ADC (`a0b1faf5`)

Даже с `analogReadMilliVolts` ESP32-S3 ADC с аттенюацией 11dB систематически
завышает ~9.5% при входном напряжении ~2V (диапазон, характерный для половины
напряжения LiPo 1S).

Измерено: 3.9V реальных → 4.27V в приложении → коэффициент ошибки 1.095.
Скорректированный множитель: `2.0 / 1.095 = 1.826`.

### `variants/heltec_wireless_paper/WirelessPaperBoard.h`

```cpp
uint16_t getBattMilliVolts() override {
  digitalWrite(PIN_ADC_CTRL, PIN_ADC_CTRL_ACTIVE);
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_VBAT_READ);
  mv /= 8;
  digitalWrite(PIN_ADC_CTRL, PIN_ADC_CTRL_INACTIVE);
  // R21=10k/R26=10k, theoretical ×2, corrected for ESP32-S3 ADC ~9.5% overcalibration
  return (mv * 1826) / 1000;
}
```

### `variants/heltec_wireless_paper/platformio.ini` — пороги батареи для companion BLE

```ini
-D BATT_MIN_MILLIVOLTS=3300      ; 3.3V = 0% на дисплее (LiPo 1S safe cutoff)
-D AUTO_SHUTDOWN_MILLIVOLTS=3300 ; программный shutdown при 3.3V
```

`BATT_MAX_MILLIVOLTS=4200` — не переопределяется (дефолт из UITask.cpp).
