# Power Optimization — ESP32 Companion Radio

## Вектор изменений

Снижение энергопотребления companion-radio нод на ESP32/ESP32-S3 в режиме idle без ущерба для UX. Ориентир: −4..7 мА в idle → +2..3 дня автономности на батарее 2000 мАч.

Все изменения условны и не затрагивают NRF52/RP2040/STM32 платформы, а также repeater-варианты.

---

## Ветвь и коммиты

| Коммит | Описание |
|--------|----------|
| `e4264450` | feat(power): companion radio power optimization for ESP32 |
| `0079fba6` | feat(power): adaptive RX gain for Heltec V3 companions |

---

## `src/helpers/esp32/SerialBLEInterface.h` / `.cpp`

### Тиред BLE реклама (fast → slow)

**Проблема:** BLE рекламировал с дефолтными интервалами ~100 мс постоянно, даже когда телефон не подключался часами.

**Решение:** два режима рекламы:

| Режим | Интервал | Условие |
|-------|----------|---------|
| Fast | 100–200 мс | Первые 60 с после включения или disconnect |
| Slow | 500–1000 мс | Более 60 с без подключения |

При отключении телефона автоматически переключается обратно в fast на 60 с для быстрого повторного подключения. Экономия: **−1..2 мА** в idle.

Новые константы:
```cpp
#define BLE_FAST_ADV_MIN          160   // 100ms (units: 0.625ms)
#define BLE_FAST_ADV_MAX          320   // 200ms
#define BLE_SLOW_ADV_MIN          800   // 500ms
#define BLE_SLOW_ADV_MAX         1600   // 1000ms
#define BLE_FAST_ADV_TIMEOUT_MS 60000   // окно fast-рекламы после disconnect
```

`BLE_FAST_ADV_TIMEOUT_MS` переопределяем через build flags.

Новое поле в классе: `unsigned long adv_slow_time` — таймер переключения.

---

### BLE TX Power

**Проблема:** ESP32 BLE по умолчанию использует +9 dBm — избыточно для телефона в кармане.

**Решение:** снижение до +3 dBm по умолчанию. Переопределяется через `-D BLE_TX_POWER=ESP_PWR_LVL_N3` (или любой `esp_power_level_t`).

```cpp
#ifndef BLE_TX_POWER
  #define BLE_TX_POWER  ESP_PWR_LVL_P3   // +3 dBm
#endif
BLEDevice::setPower(BLE_TX_POWER);
```

Экономия: **−0.5 мА**.

---

## `examples/companion_radio/main.cpp`

### WiFi Modem Sleep

После `WiFi.begin()` добавлен вызов:

```cpp
WiFi.setSleep(true);  // enable modem sleep between DTIM beacons
```

ESP32 WiFi радио засыпает между DTIM beacon-интервалами (~100 мс). Соединение и задержка не страдают. Экономия: **−5..10 мА** когда WiFi idle (нет активного TCP-трафика).

---

## `examples/companion_radio/ui-new/UITask.cpp`

### CPU Frequency Scaling

Когда дисплей выключен **и** нет активного BLE/WiFi соединения — CPU снижается с 80 МГц до 40 МГц. Восстанавливается мгновенно при пробуждении дисплея или подключении.

Проверка раз в секунду:

```cpp
#if defined(ESP_PLATFORM) && defined(ESP32_CPU_FREQ)
  bool display_on = (_display != NULL && _display->isOn());
  if (!display_on && !_connected) {
    setCpuFrequencyMhz(40);
  } else {
    setCpuFrequencyMhz(ESP32_CPU_FREQ);
  }
#endif
```

Экономия: **−0.5..1 мА** в idle. Компилируется только для ESP32 платформ.

---

## `examples/companion_radio/MyMesh.cpp` / `MyMesh.h`

### Adaptive RX Gain (адаптивный приём SX1262)

**Проблема:** `SX126X_RX_BOOSTED_GAIN=1` всегда включён — тратит ~2 мА постоянно, даже в хороших RF-условиях.

**Решение:** алгоритм динамически включает/выключает режим повышенного усиления SX1262 в зависимости от RF-среды.

Активируется флагом: `-D ADAPTIVE_RX_GAIN=1` (вместе с `-D SX126X_RX_BOOSTED_GAIN=0` как начальное значение).

#### Входные данные

| Источник | Метод | Описание |
|----------|-------|---------|
| RSSI пакетов | `radio_driver.getLastRSSI()` | EMA (α=1/8), обновляется в `logRxRaw()` |
| Шумовой пол | `radio_driver.getNoiseFloor()` | 64 сэмпла RSSI при приёме, авто-калибровка |

#### Логика принятия решений (с hysteresis)

```
каждые 5 минут:

  по EMA RSSI пакетов:
    EMA < -105 dBm → включить boost  (слабые сигналы)
    EMA > -95  dBm → выключить boost (сильные сигналы)

  по шумовому полу (перекрывает EMA если откалиброван):
    noise_floor > -100 dBm → включить boost  (шумная среда)
    noise_floor < -110 dBm и нет пакетов → выключить boost (тихая среда)
```

Hysteresis 10 dBm предотвращает осцилляцию при граничных условиях.

#### Все пороги переопределяемы через build flags

| Флаг | Умолчание | Описание |
|------|-----------|---------|
| `ADAPTIVE_GAIN_ON_RSSI` | `-105` | Включить boost если EMA ниже |
| `ADAPTIVE_GAIN_OFF_RSSI` | `-95` | Выключить boost если EMA выше |
| `ADAPTIVE_GAIN_NOISE_ON` | `-100` | Включить по шумовому полу |
| `ADAPTIVE_GAIN_NOISE_OFF` | `-110` | Разрешить выкл по шумовому полу |
| `ADAPTIVE_GAIN_CHECK_MS` | `300000` | Интервал проверки (5 мин) |

#### Новые поля в `MyMesh`

```cpp
int16_t _rx_rssi_ema;       // EMA of received packet RSSI (0 = uninitialized)
unsigned long _next_gain_check;
void adaptRxGain();
```

Экономия: **−0..2 мА** (0 в сложных RF-условиях, до 2 мА в хороших).

---

## `variants/heltec_v3/platformio.ini`

Изменения применены только к `Heltec_v3_companion_radio_ble` и `Heltec_v3_companion_radio_wifi`:

```ini
-D SX126X_RX_BOOSTED_GAIN=0   ; начальное значение — выкл (адаптив включит при необходимости)
-D ADAPTIVE_RX_GAIN=1          ; включить адаптивный алгоритм
```

---

## Итог экономии (Heltec V3 BLE companion, ориентировочно)

| Изменение | Экономия |
|-----------|---------|
| BLE slow advertising | −1..2 мА |
| BLE TX power −6 dBm | −0.5 мА |
| Adaptive RX gain (хорошие условия) | −2 мА |
| CPU 40 МГц в idle | −0.5..1 мА |
| **Итого idle** | **−4..5.5 мА** |

Baseline upstream (Heltec V3 BLE companion): ~10 мА idle → ~7 дней на 2000 мАч.  
Цель после оптимизаций: ~5..6 мА → **~14..17 дней** на 2000 мАч (при условии хороших RF-условий).
