# build.sh — суффиксы companion-сборок

**Файл:** `build.sh`

## Вектор изменений

Расширение `build_companion_firmwares()` для сборки прошивок с укороченными суффиксами.

---

## Изменение

```bash
# было:
build_all_firmwares_by_suffix "_companion_radio_usb"
build_all_firmwares_by_suffix "_companion_radio_ble"

# стало:
build_all_firmwares_by_suffix "_companion_radio_usb"
build_all_firmwares_by_suffix "_companion_radio_ble"
build_all_firmwares_by_suffix "_companion_usb"      # + новый
build_all_firmwares_by_suffix "_companion_ble"      # + новый
```

Добавлены суффиксы `_companion_usb` и `_companion_ble` для поддержки вариантов, в `platformio.ini` которых конфигурации называются без `_radio` в суффиксе.
