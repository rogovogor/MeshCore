# Changes vs upstream

Документация изменений текущей ветки относительно `main`.

| Файл | Область | Краткое описание |
|------|---------|-----------------|
| [cyrillic-support.md](cyrillic-support.md) | `src/helpers/ui/`, `variants/*/platformio.ini` | Поддержка кириллицы через `-D CYRILLIC_SUPPORT=1` для всех GFX-дисплеев |
| [ui-task.md](ui-task.md) | `examples/companion_radio/ui-new/UITask.cpp` | Адаптивная раскладка HomeScreen + переработка MsgPreview |
| [heltec-wireless-paper.md](heltec-wireless-paper.md) | `variants/heltec_wireless_paper/` | Новый `WirelessPaperBoard` с корректным измерением батареи |
| [heltec-v4-lorafem.md](heltec-v4-lorafem.md) | `variants/heltec_v4/` | Класс `LoRaFEMControl` — авто-детекция GC1109/KCT8103L (V4.2/V4.3) |
| [build.md](build.md) | `build.sh` | Добавлены суффиксы `_companion_usb` / `_companion_ble` |
| [power-optimization-esp32-companions.md](power-optimization-esp32-companions.md) | `src/helpers/esp32/`, `examples/companion_radio/`, `variants/heltec_v3/` | Оптимизация энергопотребления ESP32 companion: BLE тиред реклама, адаптивный RX gain, WiFi modem sleep, CPU scaling |
