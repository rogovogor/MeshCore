# Cyrillic Support (`-D CYRILLIC_SUPPORT=1`)

## Вектор изменений

Единая поддержка кириллицы для всех GFX-совместимых дисплеев companion-radio сборок через компилируемый флаг `-D CYRILLIC_SUPPORT=1`. Все изменения условны (`#ifdef CYRILLIC_SUPPORT`) — поведение без флага не меняется.

---

## Новые файлы

| Файл | Описание |
|------|----------|
| `src/helpers/ui/OLEDDisplayFontsRU.cpp/.h` | Шрифты ArialMT 10/16/24 с кириллическими глифами (CP1251, 224 символа) |
| `src/helpers/ui/glcdfont6x8.h` | Моноширинный шрифт 6×8 с кириллическими глифами для GFX-дисплеев |

---

## `src/helpers/ui/DisplayDriver.h`

- Добавлены поля `_font_size` и `_cursor_y_raw` в базовый класс (только при `CYRILLIC_SUPPORT`).
- `printWordWrap()` получила встроенную реализацию переноса по словам для моноширинного шрифта 6×8:
  - разбивает строку на отрезки по `max_width / (6 * _font_size)` символов,
  - предпочитает разрывать по пробелу,
  - отслеживает `_cursor_y_raw` для позиционирования строк.
- `translateUTF8ToBlocks()` расширена до полноценного конвертера UTF-8 → CP1251:
  - lead-bytes `0xD0` / `0xD1` → А–Я, а–я,
  - `0xD0/129` → Ё, `0xD1/145` → ё, украинские Є/І/Ї/Ґ,
  - неизвестные non-ASCII → `0xAE` (заглушка `®` в CP1251).
  - Без флага поведение прежнее: non-ASCII → `0xDB` (блок █ CP437).

---

## `src/helpers/ui/OLEDDisplay.cpp` / `.h`

- `drawStringInternal()`: при `CYRILLIC_SUPPORT` отключает внутренний UTF-8 рендеринг библиотеки (`utf8 = false`) — строка уже в CP1251.
- `drawStringMaxWidth()`: при `CYRILLIC_SUPPORT` использует байт напрямую вместо `fontTableLookupFunction`.
- `OLEDDisplay.h`: подключает `OLEDDisplayFontsRU.h`.

---

## GFX-совместимые дисплеи

Единообразный паттерн применён ко всем перечисленным файлам:

| Файл | Дисплей |
|------|---------|
| `src/helpers/ui/ST7789Display.cpp` | Heltec T114 (ST7789) |
| `src/helpers/ui/E213Display.cpp` | E-ink 2.13" |
| `src/helpers/ui/E290Display.cpp` | E-ink 2.90" |
| `src/helpers/ui/GxEPDDisplay.cpp` | GxEPD-совместимые e-ink |
| `src/helpers/ui/SH1106Display.cpp` | SH1106 OLED |
| `src/helpers/ui/SSD1306Display.cpp` | SSD1306 OLED |
| `src/helpers/ui/ST7735Display.cpp` | ST7735 TFT |
| `src/helpers/ui/ST7789LCDDisplay.cpp` | ST7789 LCD |

Паттерн изменений в каждом:
1. `#include "glcdfont6x8.h"` (или `OLEDDisplayFontsRU.h` для SSD1306).
2. В инициализаторе дисплея: `display->setFont(&glcdfont6x8)` (или соответствующий RU-шрифт).
3. `setTextSize(sz)`: сохраняет `_font_size = sz`.
4. `setCursor(x, y)`: сохраняет `_cursor_y_raw = y`; добавляет смещение базовой линии GFXfont `(_font_size * 7)` чтобы вызывающий код мог использовать координаты левого верхнего угла.

---

## `variants/*/platformio.ini` — добавлен флаг `-D CYRILLIC_SUPPORT=1`

Флаг добавлен в **все** companion-radio конфигурации следующих вариантов:

`ebyte_eora_s3`, `gat562_30s_mesh_kit`, `gat562_mesh_tracker_pro`, `heltec_e213`, `heltec_e290`, `heltec_t114`, `heltec_tracker`, `heltec_tracker_v2`, `heltec_v2`, `heltec_v3` (3 конфигурации), `heltec_v4`, `heltec_wireless_paper`, `ikoka_handheld_nrf`, `ikoka_stick_nrf`, `keepteen_lt1`, `lilygo_t3s3`, `lilygo_t3s3_sx1276`, `lilygo_tbeam_1w`, `lilygo_tbeam_SX1262`, `lilygo_tbeam_SX1276`, `lilygo_tbeam_supreme_SX1262`, `lilygo_tdeck`, `lilygo_techo`, `lilygo_techo_lite`, `lilygo_tlora_v2_1`, `mesh_pocket`, `meshadventurer`, `meshtiny`, `nano_g2_ultra`, `nibble_screen_connect`, `promicro`, `rak3401`, `rak4631`, `station_g2`, `thinknode_m1`, `thinknode_m2`, `thinknode_m5`, `wio-tracker-l1-eink`, `wio-tracker-l1`, `xiao_s3_wio`.
