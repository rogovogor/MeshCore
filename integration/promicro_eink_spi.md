# План: Promicro NRF52840 + WeAct Epaper 2.13" + E22/E22P

## Цель

Сформировать рабочий план для интеграции:
- плата: `promicro_nrf52840` (вариант Promicro NRF52840);
- радио: EBYTE E22-868 / E22P-868 / E22-900 (SX1262);
- дисплей: WeAct Epaper 2.13" (GxEPD2_213_B74);
- UI: энкодер + кнопка BOOT как BACK;
- GPS: опционально через `Serial1`.

План ориентирован на существующий вариант `promicro` и на текущие пины, которые были согласованы выше.

---

## Исходные данные

### Сопоставление Arduino → GPIO

- D0 = P0.08
- D1 = P0.06
- D2 = P0.17
- D3 = P0.20
- D4 = P0.22
- D5 = P0.24
- D6 = P1.00
- D7 = P0.11
- D8 = P1.04
- D9 = P1.06
- D10 = P0.09
- D11 = P0.10
- D12 = P1.11
- D13 = P1.13
- D14 = P1.15
- D15 = P0.02
- D16 = P0.29
- D17 = P0.31
- D18 = P1.01
- D19 = P1.02
- D20 = P1.05
- D21 = P0.13
- D22 = P0.15

> В этом плане принимаем это за базовую распиновку и не меняем её.

### Полная распиновка nRF52840

| nRF52840 | Arduino D | Назначение | Куда подключено |
|---|---|---|---|
| P0.02 | D15 | MOSI (SPI) | Радио SX1262 MOSI |
| P0.06 | D1 | UART TX | GPS TX |
| P0.08 | D0 | UART RX | GPS RX |
| P0.09 | D10 | RADIO RESET | SX1262 RESET |
| P0.10 | D11 | RADIO DIO1 | SX1262 DIO1 |
| P0.11 | D7 | DISPLAY RESET | Epaper RST |
| P0.13 | D21 | GPIO | свободно / доп. сенсор |
| P0.15 | D22 | GPIO | свободно / доп. функция |
| P0.17 | D2 | GPS ENABLE | GPS PWR EN |
| P0.20 | D3 | ENCODER LEFT | UI энкодер A |
| P0.22 | D4 | ENCODER RIGHT | UI энкодер B |
| P0.24 | D5 | DISPLAY CS | Epaper CS |
| P0.29 | D16 | MISO (SPI) / RADIO BUSY | Радио SX1262 MISO, BUSY |
| P0.31 | D17 | BUZZER | Buzzer (PIN_BUZZER) |
| P1.00 | D6 | BUTTON BACK | BOOT как BACK |
| P1.01 | D18 | SPI1 SCK | Epaper SCK |
| P1.02 | D19 | SPI1 MISO | Epaper MISO |
| P1.04 | D8 | DISPLAY BUSY | Epaper BUSY |
| P1.05 | D20 | SPI1 MOSI | Epaper MOSI |
| P1.11 | D12 | DISPLAY DC | Epaper DC |
| P1.13 | D13 | RADIO CS | SX1262 NSS |
| P1.15 | D14 | RADIO SCK | SX1262 SCLK |

---

## Архитектура подключения

### Радио E22 / E22P

- Радио подключается через стандартную `SPI` шину, как в варианте `promicro`.
- Пины радио:
  - `SCLK` = D14 = P1.15
  - `MOSI` = D15 = P0.02
  - `MISO` = D16 = P0.29
  - `NSS/CS` = D13 = P1.13
  - `RESET` = D10 = P0.09
  - `DIO1` = D11 = P0.10
  - `BUSY` = D16 = P0.29 (если используется как `P_LORA_BUSY`)

### Дисплей WeAct Epaper 2.13"

- Дисплей должен работать по `SPI1`, чтобы не мешать радио на `SPI`.
- Используем простой программный вариант из текущего проекта: `GxEPDDisplay` + `GxEPD2_213_B74`.
- Пины дисплея:
  - `CS` = D5 = P0.24
  - `DC` = D12 = P1.11
  - `RST` = D7 = P0.11
  - `BUSY` = D8 = P1.04
- SPI1 физические линии:
  - `SCK` = D18 = P1.01
  - `MOSI` = D20 = P1.05
  - `MISO` = D19 = P1.02 (обычно для e-ink не обязательно, но лучше подключить)

### UI и GPS

- `ENCODER_LEFT` = D3 = P0.20
- `ENCODER_RIGHT` = D4 = P0.22
- `ENCODER_PRESS` = D9 = P1.06
- `BOOT` как `BACK` = D6 = P1.00
- `GPS_RX` = D0 = P0.08
- `GPS_TX` = D1 = P0.06
- `GPS_ENABLE` = D2 = P0.17

---

## Почему так

- В варианте `promicro` радио уже выделено на `SPI`, и это самая стабильная схема.
- WeAct Epaper — SPI-дисплей, но он не должен «делить» с радио тот же физический SPI, иначе потребуется жёсткая синхронизация CS и управление режимами обоих устройств.
- Использование `SPI1` для экрана даёт явное разделение шины и уменьшает риск конфликтов.
- `Wire`/I2C здесь не используется, потому что экран e-ink по спецификации работает по SPI и вариант promicro уже использует D6/D7 для кнопки и RESET.

---

## Отличие E22 от E22P

### Общие черты

- Оба модуля используют SX1262.
- Структура подключения в проекте остается одинаковой.
- Вариант `promicro` не изменяет физические линии, только поведение прошивки.

### Ключевая разница

- `E22` работает как обычный SX1262-модуль.
- `E22P` требует дополнительного программного патча регистра RX для надежного приёма.
- Этот патч в MeshCore уже выражен через `SX126X_REGISTER_PATCH`.

### Что нужно сделать в прошивке

- Для `E22`: обычные флаги `USE_SX1262`, `RADIO_CLASS=CustomSX1262`, `WRAPPER_CLASS=CustomSX1262Wrapper`.
- Для `E22P`: дописать флаг `-D SX126X_REGISTER_PATCH=1`.

### Где применяется патч

В `src/helpers/radiolib/CustomSX1262.h`:
- после успешной инициализации SX1262 читаем регистр `0x8B5`
- устанавливаем младший бит
- записываем обратно

Это именно та правка, которая заложена в PR #1937.

---

## Что должно быть реализовано

### 1. Вариант платы

Создать вариант `promicro_eink_spi` на базе `promicro` со следующими особенностями:
- фиксированная распиновка `promicro_nrf52840`;
- отключён `Wire` / I2C;
- экран работает через `SPI1`;
- радио работает через `SPI`;
- GPS через `Serial1`.

### 2. Пиновые определения

Добавить или проверить в вариантах следующие обозначения:

- `PIN_DISPLAY_CS = D5`
- `PIN_DISPLAY_DC = D12`
- `PIN_DISPLAY_RST = D7`
- `PIN_DISPLAY_BUSY = D8`
- `PIN_SPI1_SCK = D18`
- `PIN_SPI1_MOSI = D20`
- `PIN_SPI1_MISO = D19`
- `PIN_BUZZER = D17`
- `P_LORA_SCLK = D14`
- `P_LORA_MOSI = D15`
- `P_LORA_MISO = D16`
- `P_LORA_NSS = D13`
- `P_LORA_RESET = D10`
- `P_LORA_DIO_1 = D11`
- `P_LORA_BUSY = D16`

> `PIN_BUZZER` используется в других вариантах проекта (`rak_wismesh_tag` и другие), так что можно взять реализацию драйвера из существующих примеров.

### 3. Дисплейный драйвер

- `DISPLAY_CLASS = GxEPDDisplay`
- `EINK_DISPLAY_MODEL = GxEPD2_213_B74`
- `PIN_DISPLAY_*` на уровне варианта
- `GxEPDDisplay.cpp` должен использовать `SPI1.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS)`

### 4. Радиодрайвер

- `RADIO_CLASS = CustomSX1262`
- `WRAPPER_CLASS = CustomSX1262Wrapper`
- `USE_SX1262`
- `LORA_TX_POWER` и `SX126X_CURRENT_LIMIT` на уровне варианта
- для E22P опция `-D SX126X_REGISTER_PATCH=1`

### 5. Функциональные опции

- `UI_HAS_JOYSTICK`
- `ENV_INCLUDE_GPS=1` для GPS
- `PIN_GPS_RX`, `PIN_GPS_TX`, `PIN_GPS_EN`
- `BUTTON_PIN = D6` как BACK

---

## Программные цели

### Базовая цель

Собрать прошивку, которая:
- инициализирует SX1262 на `SPI`;
- инициализирует e-ink на `SPI1`;
- не включает I2C;
- работает с энкодером и кнопкой BOOT;
- поддерживает GPS, если он подключён.

### Разделение E22 / E22P

- `E22` — стандартная сборка.
- `E22P` — сборка с `SX126X_REGISTER_PATCH`.

---

## Предлагаемая структура задачи

1. Создать папку `variants/promicro_eink_spi` или аналогичный вариант.
2. Скопировать основу `variants/promicro` и минимально адаптировать под e-ink.
3. Описать `variant.h` и `variant.cpp` со всеми согласованными пинами.
4. Настроить `board` / `target` для `Promicro_WeAct`.
5. Описать `platformio.ini` окружения для:
   - `ProMicro_WeAct_E22`
   - `ProMicro_WeAct_E22P`
   - дополнительные окружения `companion_radio_ble`, `companion_radio_usb`, `repeater`, `room_server`
6. Убедиться, что `GxEPDDisplay` работает через `SPI1` и не использует `Wire`.
7. Убедиться, что `CustomSX1262` и `CustomSX1262Wrapper` используют `SPI` без конфликтов.
8. Протестировать сборку по крайней мере для `ProMicro_WeAct_E22` и `ProMicro_WeAct_E22P`.

---

## Тестовый план

- `pio run -e ProMicro_WeAct_E22`
- `pio run -e ProMicro_WeAct_E22P`
- `pio run -e ProMicro_WeAct_E22_companion_radio_ble`
- `pio run -e ProMicro_WeAct_E22P_868_companion_radio_ble`

Контрольные моменты:
- экран инициализируется и выводит первый кадр;
- радио инициализируется успешно;
- `E22P` сборка содержит `SX126X_REGISTER_PATCH`;
- `Button BOOT` работает как BACK;
- энкодер реагирует на LEFT/RIGHT/PRESS;
- GPS запускается по `Serial1`.

---

## Примечания

- Для `WeAct Epaper 2.13"` обязательно использовать `SPI1`, а не `Wire`.
- Если в будущем потребуется один общий SPI для радио и дисплея — это отдельная задача, пока оставляем их разделёнными.
- Разница `E22P` vs `E22` в этом плане — только программная настройка флага патча, не аппаратная.
- Пин `D17` в этой распиновке уже занят как `P0.31`, поэтому в плане он входит в общее маппирование и не используется для e-ink или радио.
