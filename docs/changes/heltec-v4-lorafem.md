# Heltec V4 — абстракция управления LoRa FEM (GC1109 / KCT8103L)

## Вектор изменений

Вынесение управления Front-End Module (FEM) из `HeltecV4Board` в отдельный класс `LoRaFEMControl` с авто-детекцией типа FEM. Это поддерживает как V4.2 (GC1109), так и V4.3 (KCT8103L).

---

## Новые файлы

### `variants/heltec_v4/LoRaFEMControl.h` / `.cpp`

Класс `LoRaFEMControl` инкапсулирует:

| Метод | Описание |
|-------|----------|
| `init()` | Включение LDO питания FEM, авто-детекция типа (GC1109 vs KCT8103L), настройка пинов |
| `setTxModeEnable()` | Переключение FEM в режим TX |
| `setRxModeEnable()` | Переключение FEM в режим RX |
| `setRxModeEnableWhenMCUSleep()` | Настройка RTC GPIO hold для работы LNA во время deep sleep |
| `setSleepModeEnable()` | Выключение FEM (режим сна) |
| `getFEMType()` | Возвращает `GC1109_PA` или `KCT8103L_PA` |

**Авто-детекция** (`init()`): пин `P_LORA_KCT8103L_PA_CSD` (GPIO2) настраивается как INPUT и проверяется уровень:
- HIGH → KCT8103L (V4.3, внутренний pull-up)
- LOW → GC1109 (V4.2, внутренний pull-down)

---

## `variants/heltec_v4/HeltecV4Board.h`

- Добавлено `#include "LoRaFEMControl.h"`.
- Добавлено поле `LoRaFEMControl loRaFEMControl`.

---

## `variants/heltec_v4/HeltecV4Board.cpp`

- `begin()`: прямые операции с GPIO пинами FEM заменены на `loRaFEMControl.init()`.
- `onBeforeTransmit()`: `digitalWrite(P_LORA_PA_TX_EN, HIGH)` → `loRaFEMControl.setTxModeEnable()`.
- `onAfterTransmit()`: `digitalWrite(P_LORA_PA_TX_EN, LOW)` → `loRaFEMControl.setRxModeEnable()`.
- `enterDeepSleep()`: прямые `rtc_gpio_hold_en` заменены на `loRaFEMControl.setRxModeEnableWhenMCUSleep()`.
- `getManufacturerName()`: теперь различает V4.2 и V4.3:
  - `KCT8103L_PA` → `"Heltec V4.3 TFT"` / `"Heltec V4.3 OLED"`
  - `GC1109_PA` → `"Heltec V4 TFT"` / `"Heltec V4 OLED"`

---

## Merge: PR #1867 (Quency-D/dev-heltec-v4.3)

Изменения содержат мёрж PR `#1867` из ветки `Quency-D/dev-heltec-v4.3`, добавившего поддержку Heltec V4.3 с FEM KCT8103L.
