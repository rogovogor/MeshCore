# Fix: BLE Crash on ESP32-S3 (CPU Scaling + Light Sleep)

## Симптомы

Нода `Heltec_v3_companion_radio_ble` (и любая другая ESP32-S3 BLE companion) уходила в
бесконечный ребут каждые ~3 секунды:

```
BLE: SerialBLEInterface::disable
Guru Meditation Error: Core 0 panic'ed (Interrupt wdt timeout on CPU0)
  #0  hci_le_set_adv_en_cmd_handler
  #4  ble_try_turn_on_pll_track
  #5  btdm_controller_task
```

Нода на eink (E213 BLE) под те же условия не падала.

---

## Причина 1 — CPU масштабирование до 40 MHz при активном BLE

**ESP32-S3 поддерживает BLE только при CPU ≥ 80 MHz.**
Вызов `setCpuFrequencyMhz(40)` при активном BLE дестабилизирует контроллер:
следующий вызов `advertising->stop()` попадает в момент `ble_try_turn_on_pll_track`
и блокирует interrupt handler до срабатывания Interrupt WDT.

### Цепочка для V3

1. `display.begin()` → SSD1306 на I2C → I2C ошибки → возвращает `false` → `disp = NULL`
2. `UITask::begin(NULL, ...)` → `_display = NULL`
3. В `UITask::loop()`: `display_on = (_display != NULL && ...) = false`, `_connected = false`
4. → `setCpuFrequencyMhz(40)` вызывается на первом же `loop()` (~3 секунды от старта)
5. BLE стек дестабилизируется → WDT crash

### Почему E213 не падала

E213 использует e-ink дисплей на **SPI** — инициализируется надёжно → `_display != NULL`,
`_display->isOn() = true` → `display_on = true` → 40 MHz не устанавливается.

### Фикс (`UITask.cpp`)

```cpp
if (!display_on && !_connected) {
#ifdef BLE_PIN_CODE
  setCpuFrequencyMhz(80);   // BLE minimum on ESP32-S3; 40 MHz crashes BLE stack
#else
  setCpuFrequencyMhz(40);
#endif
```

---

## Причина 2 — `disable()` перед light sleep

`checkLightSleep()` вызывал `_serial->disable()` → `advertising->stop()` → та же точка краша.

На ESP32-S3 `esp_light_sleep_start()` автоматически паузирует BLE радио.
Явный `stop()` перед сном не нужен и опасен.

### Фикс (`MyMesh.cpp`)

Убраны вызовы `_serial->disable()` / `_serial->enable()` из `checkLightSleep()`.
BLE stack управляет собой во время light sleep без вмешательства приложения.

---

## Изменённые файлы

| Файл | Изменение |
|---|---|
| `examples/companion_radio/ui-new/UITask.cpp` | CPU idle floor: 80 MHz при `BLE_PIN_CODE` |
| `examples/companion_radio/MyMesh.cpp` | Убраны `disable()`/`enable()` из `checkLightSleep()` |

---

## Правило

> На ESP32-S3 с активным BLE **никогда не снижать CPU ниже 80 MHz**.
> Вызовы `advertising->stop()` / `start()` из задачи приложения безопасны только
> когда BLE контроллер не занят radio операцией (PLL track, TX window).
