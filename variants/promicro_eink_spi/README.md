# ProMicro NRF52840 + WeAct Epaper 2.13" + EBYTE E22/E22P или RA-62

Вариант `promicro_eink_spi` — companion radio / repeater на базе:

- **MCU:** ProMicro NRF52840 (совместимый с Adafruit Feather nRF52840)
- **Радио:** EBYTE E22-868M30S / E22-900M30S / E22P-868M30S / E22P-900M30S (SX1262) **или** AI-Thinker RA-62 (LLCC68)
- **Дисплей:** WeAct Studio Epaper 2.13" (250×122, GxEPD2_213_B74)
- **UI:** энкодер (LEFT / RIGHT / PRESS) + кнопка BOOT как BACK
- **GPS:** опционально, через Serial1

> **Модули E22 M30S** — это SPI-версии без UART и без пинов управления режимом (M0/M1).
> Аппаратно E22 и E22P подключаются одинаково; E22P отличается только прошивкой.

---

## Распиновка

### Arduino D# → GPIO nRF52840

| Arduino | GPIO    | Назначение          | Куда подключено              |
|---------|---------|---------------------|------------------------------|
| D0      | P0.08   | UART RX (Serial1)   | GPS TX                       |
| D1      | P0.06   | UART TX (Serial1)   | GPS RX                       |
| D2      | P0.17   | GPS ENABLE          | GPS EN (HIGH = вкл)          |
| D3      | P0.20   | ENCODER LEFT        | Энкодер A                    |
| D4      | P0.22   | ENCODER RIGHT       | Энкодер B                    |
| D5      | P0.24   | DISPLAY CS          | Epaper CS                    |
| D6      | P1.00   | BUTTON BACK         | BOOT кнопка → BACK           |
| D7      | P0.11   | DISPLAY RST         | Epaper RST                   |
| D8      | P1.04   | DISPLAY BUSY        | Epaper BUSY                  |
| D9      | P1.06   | ENCODER PRESS       | Энкодер SW (кнопка)          |
| D10     | P0.09   | RADIO RESET         | SX1262/LLCC68 NRESET         |
| D11     | P0.10   | RADIO DIO1          | SX1262/LLCC68 DIO1           |
| D12     | P1.11   | DISPLAY DC          | Epaper DC                    |
| D13     | P1.13   | RADIO CS            | SX1262/LLCC68 NSS            |
| D14     | P1.15   | RADIO SCK (SPI)     | SX1262/LLCC68 SCK            |
| D15     | P0.02   | RADIO MOSI (SPI)    | SX1262/LLCC68 MOSI           |
| D16     | P0.29   | RADIO MISO (SPI)    | SX1262/LLCC68 MISO           |
| D17     | P0.31   | VBAT ADC (AIN7)     | Делитель напряжения батареи  |
| D18     | P1.01   | SPI1 SCK            | Epaper SCK                   |
| D19     | P1.02   | RADIO BUSY          | SX1262/LLCC68 BUSY           |
| D20     | P1.05   | SPI1 MOSI           | Epaper MOSI (DIN)            |
| D21     | P0.13   | VCC_EN (радио)      | Питание радио (HIGH = вкл)   |
| D22     | P0.15   | BUZZER / LED        | Buzzer + LED_BUILTIN         |

> **D19** ранее был SPI1 MISO для дисплея, но e-paper работает только на запись —
> MISO дисплея не нужен. Пин перепрофилирован под RADIO BUSY.
> SPI1 MISO физически остаётся незадействованным (не подключать к дисплею).

---

## Схема подключения

### Радио EBYTE E22 M30S / E22P M30S (SX1262) ↔ ProMicro (SPI)

```
ProMicro D14 (P1.15) ──── E22 SCK
ProMicro D15 (P0.02) ──── E22 MOSI
ProMicro D16 (P0.29) ──── E22 MISO
ProMicro D19 (P1.02) ──── E22 BUSY      ← отдельный провод, не совмещать с MISO
ProMicro D13 (P1.13) ──── E22 NSS (CS)
ProMicro D10 (P0.09) ──── E22 NRESET
ProMicro D11 (P0.10) ──── E22 DIO1
ProMicro D21 (P0.13) ──── E22 VCC_EN (через транзистор/MOSFET → E22 VCC)
3.3V ───────────────────── E22 VCC  (если без транзистора)
GND ────────────────────── E22 GND
```

> **E22P M30S** требует прошивку с флагом `-D SX126X_REGISTER_PATCH=1`
> (env `ProMicro_WeAct_E22P_*`). Аппаратно E22 и E22P подключаются одинаково.

### Радио RA-62 (LLCC68) ↔ ProMicro (SPI)

Распиновка идентична E22 M30S:

```
ProMicro D14 (P1.15) ──── RA-62 SCK
ProMicro D15 (P0.02) ──── RA-62 MOSI
ProMicro D16 (P0.29) ──── RA-62 MISO
ProMicro D19 (P1.02) ──── RA-62 BUSY
ProMicro D13 (P1.13) ──── RA-62 NSS (CS)
ProMicro D10 (P0.09) ──── RA-62 NRESET
ProMicro D11 (P0.10) ──── RA-62 DIO1
ProMicro D21 (P0.13) ──── RA-62 VCC_EN (через транзистор/MOSFET → RA-62 VCC)
3.3V ───────────────────── RA-62 VCC  (если без транзистора)
GND ────────────────────── RA-62 GND
```

> LLCC68 не имеет TCXO; прошивка задаёт `SX126X_DIO3_TCXO_VOLTAGE=0.0f`.
> RA-62 не поддерживает SF12 и ограничен BW ≥ 125 кГц при SF ≥ 10.

### Дисплей WeAct Epaper 2.13" ↔ ProMicro (SPI1)

```
ProMicro D18 (P1.01) ──── Epaper SCK
ProMicro D20 (P1.05) ──── Epaper MOSI (DIN)
ProMicro D5  (P0.24) ──── Epaper CS
ProMicro D12 (P1.11) ──── Epaper DC
ProMicro D7  (P0.11) ──── Epaper RST
ProMicro D8  (P1.04) ──── Epaper BUSY
3.3V ───────────────────── Epaper VCC
GND ────────────────────── Epaper GND
```

> Epaper MISO не подключается — дисплей работает только на запись.
> D19 используется как RADIO BUSY (см. распиновку выше).

### Энкодер (KY-040 или аналог)

```
ProMicro D3 (P0.20) ──── Энкодер CLK (A)
ProMicro D4 (P0.22) ──── Энкодер DT  (B)
ProMicro D9 (P1.06) ──── Энкодер SW  (кнопка нажатия)
3.3V ────────────────── Энкодер +
GND ─────────────────── Энкодер GND
```

> Все три пина подтянуты к питанию внутри MCU (`INPUT_PULLUP`).
> Внешние резисторы не нужны.

### Кнопка BACK

Функцию BACK выполняет встроенная кнопка **BOOT** на плате ProMicro.
Дополнительных компонентов не требуется.

```
BOOT кнопка (D6 / P1.00) — уже на плате, INPUT_PULLUP
```

### Buzzer

Buzzer подключается на **D22 (P0.15)** — это также `LED_BUILTIN` на плате.
LED будет мигать вместе с buzzer'ом (визуальная индикация).

```
ProMicro D22 (P0.15) ──── 100 Ω ──── Buzzer + (пьезо)
GND ──────────────────────────────── Buzzer −
```

### Батарея — делитель напряжения

На **D17 (P0.31 = AIN7)** подключается делитель для измерения VBAT.
Для LiPo (макс. 4.2 В) делитель 100 кОм / 100 кОм даёт ≤ 2.1 В на АЦП
(в пределах 3.6 В — безопасно).

```
VBAT ──── 100 кОм ──── D17 (P0.31) ──── 100 кОм ──── GND
```

Формула расчёта (SAADC nRF52840: опора 0.6 В, усиление 1/6 → шкала 3.6 В, 12 бит):

```
VBAT_мВ = analogRead() × ADC_MULTIPLIER
Теоретически: ADC_MULTIPLIER = (3600 / 4095) × 2 ≈ 1.76
```

Начальное значение `ADC_MULTIPLIER = 1.73f` — немного занижено, требует
калибровки по известному напряжению через `AT+ADCMULT=<value>` или напрямую в коде.

> Мультипликатор не равен 2.0: SAADC nRF52840 использует шкалу 3.6 В (не 3.3 В),
> а функция возвращает результат в мВ из сырых отсчётов (0–4095).

### GPS (опционально)

```
ProMicro D0 (P0.08) ──── GPS TX (Serial1 RX)
ProMicro D1 (P0.06) ──── GPS RX (Serial1 TX)
ProMicro D2 (P0.17) ──── GPS EN  (HIGH = включить питание GPS)
3.3V / 5V ──────────── GPS VCC (см. даташит модуля)
GND ────────────────── GPS GND
```

Скорость: 9600 бод (по умолчанию). Для изменения добавьте `-D GPS_BAUD_RATE=...`

---

## Питание радио — D21 (P0.13, VCC_EN)

`SX126X_POWER_EN` (D21 / P0.13) управляет питанием **только радиомодуля**
через внешний транзистор или MOSFET.

**Текущая последовательность в `PromicroEinkBoard::begin()`:**
1. `pinMode(D21, OUTPUT)`
2. `digitalWrite(D21, HIGH)` — включить питание радио
3. `delay(10)` — пауза для стабилизации SX1262/LLCC68

**Дисплей** питается от постоянной шины 3.3 В платы и **не** управляется через D21.

**Если планируется питать дисплей тоже от D21:**
- Увеличьте задержку до ≥ 100 мс (e-paper требует больше времени на старт).
- Учтите, что `powerOff()` (→ `sd_power_system_off()`) отключит питание и дисплея:
  он не успеет «припарковать» экран корректно.
- Добавьте явную команду `display.hibernate()` перед `powerOff()`.

---

## Архитектура SPI

| Шина  | Назначение | SCK  | MOSI | MISO        |
|-------|-----------|------|------|-------------|
| SPI   | Радио     | D14  | D15  | D16         |
| SPI1  | Дисплей   | D18  | D20  | не подключен|

BUSY радио — **D19** (отдельный пин, не совмещён с SPI MISO).
BUSY дисплея — **D8** (отдельный пин).
Две независимые SPI-шины исключают конфликты между радиомодулем и дисплеем.
Wire (I2C) в этом варианте не инициализируется.

---

## Сборка прошивки

### Окружения

| Окружение                                 | Описание                             |
|-------------------------------------------|--------------------------------------|
| `ProMicro_WeAct_E22_companion_radio_ble`  | Companion radio, E22 M30S, BLE       |
| `ProMicro_WeAct_E22_companion_radio_usb`  | Companion radio, E22 M30S, USB       |
| `ProMicro_WeAct_E22_repeater`             | Репитер, E22 M30S                    |
| `ProMicro_WeAct_E22_room_server`          | Room server, E22 M30S                |
| `ProMicro_WeAct_E22P_companion_radio_ble` | Companion radio, E22P M30S, BLE      |
| `ProMicro_WeAct_E22P_companion_radio_usb` | Companion radio, E22P M30S, USB      |
| `ProMicro_WeAct_E22P_repeater`            | Репитер, E22P M30S                   |
| `ProMicro_WeAct_E22P_room_server`         | Room server, E22P M30S               |
| `ProMicro_WeAct_RA62_companion_radio_ble` | Companion radio, RA-62 (LLCC68), BLE |
| `ProMicro_WeAct_RA62_companion_radio_usb` | Companion radio, RA-62 (LLCC68), USB |
| `ProMicro_WeAct_RA62_repeater`            | Репитер, RA-62 (LLCC68)              |
| `ProMicro_WeAct_RA62_room_server`         | Room server, RA-62 (LLCC68)          |

### Сборка

```bash
# Companion radio с BLE, модуль E22 M30S
pio run -e ProMicro_WeAct_E22_companion_radio_ble

# Companion radio с BLE, модуль E22P M30S (с RX-патчем)
pio run -e ProMicro_WeAct_E22P_companion_radio_ble

# Companion radio с BLE, модуль RA-62 (LLCC68)
pio run -e ProMicro_WeAct_RA62_companion_radio_ble

# Прошивка через USB (UF2 / nrfutil)
pio run -e ProMicro_WeAct_E22_companion_radio_ble --target upload
```

---

## Реализация

### Файлы варианта

| Файл                     | Назначение                                                    |
|--------------------------|---------------------------------------------------------------|
| `variant.h`              | Определения пинов, SPI/SPI1, extern SCK/MISO/MOSI            |
| `variant.cpp`            | Таблица `g_ADigitalPinMap[]`, определения SCK/MISO/MOSI      |
| `PromicroEinkBoard.h`    | Класс платы: пины радио, дисплея, энкодера, АЦП батареи      |
| `PromicroEinkBoard.cpp`  | `begin()`: питание радио, кнопка, без Wire                    |
| `target.h`               | Объявления: board, radio_driver, display, кнопки             |
| `target.cpp`             | Инициализация радио (SPI), дисплея (SPI1), GPS, энкодера     |
| `platformio.ini`         | 12 окружений: E22, E22P, RA-62                                |

### Особенности

**RADIO BUSY на D19:**
BUSY радио вынесен на D19 (P1.02) — отдельный пин, не совмещённый с MISO.
Физически D19 совпадает с PIN_SPI1_MISO в variant.h, но SPI1 никогда не читает
данные (e-paper — write-only), поэтому конфликта нет. E-paper MISO не подключается.

**Buzzer на D22 / LED_BUILTIN:**
D22 = P0.15 — это `LED_BUILTIN` на плате. Buzzer подключён туда же через
100 Ом. При воспроизведении тона LED мигает вместе с buzzer'ом.
Это освобождает D17 (P0.31 = AIN7) — единственный свободный аналоговый
пин — под делитель напряжения батареи.

**АЦП батареи на D17:**
Требует внешнего делителя 100 кОм / 100 кОм. Теоретический `ADC_MULTIPLIER ≈ 1.76`
(SAADC nRF52840, шкала 3.6 В, 12 бит, делитель ×2). Начальное значение 1.73f —
немного занижено; точную калибровку нужно провести по известному напряжению.
`setAdcMultiplier()` позволяет менять коэффициент в рантайме.

**Радио (SPI):**
`CustomSX1262::std_init()` (и `CustomLLCC68::std_init()`) вызывает
`SPI.setPins(D16, D14, D15)` перед `SPI.begin()`, поэтому пины радио
не зависят от `PIN_SPI_*` в `variant.h`.

**Дисплей (SPI1):**
`GxEPDDisplay::begin()` вызывает `SPI1.begin()` (без аргументов на nRF52),
который берёт пины `PIN_SPI1_SCK/MOSI` из `variant.h` (D18/D20).
Затем `display.epd2.selectSPI(SPI1, ...)` переключает GxEPD2 на эту шину.
MISO дисплея не используется и не подключается.

**Wire:**
В `PromicroEinkBoard::begin()` `Wire.begin()` не вызывается.
I2C-устройства (OLED, RTC, датчики) в этом варианте не используются.

**SCK / MISO / MOSI (глобальные):**
GxEPD2 компилирует `GxEPD2_1248c.cpp` (12.48" панель) даже на nRF52,
а тот использует глобальные константы `SCK`, `MISO`, `MOSI`.
В `variant.cpp` они объявлены равными пинам SPI1 (D18/D19/D20) — это
корректный dummy-компромисс, т.к. `GxEPD2_1248c` никогда не инстанциируется.

**E22P M30S:**
Отличие от E22 M30S — только программное: флаг `-D SX126X_REGISTER_PATCH=1`
заставляет `CustomSX1262::std_init()` после инициализации читать регистр
`0x8B5`, устанавливать младший бит и записывать обратно (патч RX-фильтра).
Аппаратно E22 и E22P подключаются одинаково.

**RA-62 (LLCC68):**
LLCC68 — упрощённая версия SX1262 без TCXO и с ограничением SF5–SF11.
Прошивка использует `CustomLLCC68` / `CustomLLCC68Wrapper` и явно задаёт
`SX126X_DIO3_TCXO_VOLTAGE=0.0f`. Пиновая разводка платы не меняется.
