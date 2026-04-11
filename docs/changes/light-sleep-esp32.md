# Light Sleep для ESP32-S3 Companion Radio

## Вектор изменений

Снижение энергопотребления companion radio через автоматический вход в light sleep
при бездействии (нет LoRa активности, BLE не передаёт данные).

**Light sleep vs deep sleep:**
- CPU останавливается, RAM сохраняется — выполнение продолжается с той же строки
- Нет перезагрузки, нет потери состояния
- Ток в light sleep: ~1-3 mA vs ~80-100 mA активный режим

**Поддерживаемые платформы:** только ESP32-S3
(защита `#if defined(CONFIG_IDF_TARGET_ESP32S3)` в `ESP32Board.h`)

---

## Источники пробуждения

| Источник | GPIO | Условие |
|---|---|---|
| LoRa пакет | `P_LORA_DIO_1` (GPIO14) | DIO1 HIGH — немедленный выход из sleep |
| Кнопка пользователя | `PIN_USER_BTN` (GPIO0) | любой HIGH — выход из sleep |
| BLE connection event | — | ESP32-S3 автоматически просыпается по connection interval (без явной регистрации) |
| Таймер | — | fallback через `COMPANION_LIGHT_SLEEP_SECS` секунд |

При подключённом телефоне BLE соединение **сохраняется** во время sleep —
стек просыпается на connection events, телефон не замечает разрыва.

---

## Поведение

### При BLE disconnected:
1. Через `COMPANION_IDLE_BEFORE_SLEEP_MS` мс без активности
2. BLE advertising останавливается (`_serial->disable()`)
3. Нода уходит в light sleep
4. Просыпается по DIO1 (LoRa пакет) / кнопке / таймеру
5. BLE advertising возобновляется (`_serial->enable()`)
6. Цикл повторяется

### При BLE connected:
1. Через `COMPANION_IDLE_BEFORE_SLEEP_MS` мс без LoRa активности
2. Нода уходит в light sleep (advertising не трогается)
3. BLE connection events регулярно будят чип (прозрачно для телефона)
4. LoRa пакет → немедленное пробуждение → обработка → снова sleep

---

## Изменённые файлы

### `src/helpers/ESP32Board.h` — `enterLightSleep()`

Добавлены wakeup источники:
- `PIN_USER_BTN` в ext1 маску (если определён и RTC-capable)
- BLE connection events будят чип автоматически (ESP32-S3 light sleep — без явного вызова `esp_sleep_enable_bt_wakeup`, которого нет в Arduino framework)

### `examples/companion_radio/MyMesh.h`

```cpp
#if defined(COMPANION_IDLE_BEFORE_SLEEP_MS) && defined(ESP32)
  unsigned long _last_activity_ms;
  void checkLightSleep();
#endif
```

### `examples/companion_radio/MyMesh.cpp`

- `_last_activity_ms` обновляется при каждом входящем LoRa пакете (`logRxRaw`)
- `checkLightSleep()` — проверяет таймаут и управляет входом/выходом из sleep

### `examples/companion_radio/main.cpp`

`checkLightSleep()` вызывается в конце `loop()`.

### `variants/*/platformio.ini` — 20 ESP32-S3 платформ

```ini
-D COMPANION_IDLE_BEFORE_SLEEP_MS=30000   ; 30с бездействия → sleep
-D COMPANION_LIGHT_SLEEP_SECS=30          ; таймер fallback: 30с (потом снова sleep)
```

---

## Платформы

ESP32-S3: `heltec_v3`, `heltec_v4`, `heltec_wireless_paper`, `heltec_e213`, `heltec_e290`,
`heltec_tracker`, `heltec_tracker_v2`, `heltec_t190`, `heltec_ct62`, `ebyte_eora_s3`,
`lilygo_t3s3`, `lilygo_tbeam_supreme_SX1262`, `lilygo_tdeck`, `nibble_screen_connect`,
`rak3112`, `station_g2`, `thinknode_m2`, `thinknode_m5`, `xiao_c3`, `xiao_s3_wio`

Classic ESP32 (`heltec_v2`, `lilygo_tbeam_SX1276` и др.) — исключены (legacy).
