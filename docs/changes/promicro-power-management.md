# Управление питанием nRF52 для ProMicro

## Краткое описание

Включён `NRF52_POWER_MANAGEMENT` для:

- `variants/promicro` (Nice!nano / ProMicro nRF52840)

Реализованы три уровня защиты:

1. **Boot protection** — не запускается если батарея ниже 3350 мВ при старте.
2. **Runtime low-voltage shutdown** — LPCOMP DOWN ISR вызывает контролируемый SYSTEMOFF при разрядке ниже ~3.0 В и сохраняет причину выключения.
3. **Voltage-recovery wake** — LPCOMP UP + VBUS wake обеспечивают пробуждение после восстановления заряда (~3.9 В) или при подключении USB.

## Конфигурация

Оба варианта используют одинаковые параметры:

| Параметр | Значение | Назначение |
|----------|----------|------------|
| `PWRMGT_VOLTAGE_BOOTLOCK` | 3350 | Boot lock порог (мВ), АЦП-измерение |
| `PWRMGT_LPCOMP_AIN` | 7 | AIN7 = P0.31 = D17 (батарея через делитель) |
| `PWRMGT_LPCOMP_REFSEL` | 12 | LPCOMP UP: 9/16 VDD → пробуждение ~3.9 В |
| `PWRMGT_LPCOMP_LOW_REFSEL` | 11 | LPCOMP DOWN: 7/16 VDD → shutdown ~3.0 В |

## Аппаратный контекст

Батарея подключена через резистивный делитель к `D17` → `P0.31` → `AIN7`.
Калиброванный множитель АЦП: `ADC_MULTIPLIER = 1.815f` (compile-time), типичное
реальное значение ~1.880 (runtime-калибровка через `set adc_multiplier`).

**Особенность платы:** нRF52840 питается непосредственно от батареи (VDD ≈ V_bat),
DC-DC конвертер включён (`NRF52BoardDCDC`). LPCOMP использует VDD как опорное
напряжение, поэтому его пороги масштабируются с напряжением батареи.
Калибровка порогов проведена экспериментально:

| REFSEL | Дробь VDD | Наблюдаемый порог V_bat |
|--------|-----------|------------------------|
| 11 | 7/16 | ~3.0 В |
| 12 | 9/16 | ~3.9 В |
| 4 | 5/8 | ~4.15 В |

АЦП использует внутренний опорный 0.6 В (независимо от VDD) — поэтому boot
protection по АЦП всегда даёт абсолютное измерение, в отличие от LPCOMP.

## Логика работы

### Полный жизненный цикл

```
 [загрузка]
     │
     ▼
 checkBootVoltage() — АЦП-измерение
     │
     ├─ USB подключён ──────────────────────────────► [загрузка продолжается]
     │                                                        │
     ├─ V_bat ≥ 3350 мВ ───────────────────────────► [загрузка продолжается]
     │                                                        │
     └─ V_bat < 3350 мВ ──► initiateShutdown(BOOT_PROTECT)   │
                                      │                       │
                                      ▼                       ▼
                             configureVoltageWake()  configureLowVoltageAlert()
                             (LPCOMP UP, REFSEL=12)  (LPCOMP DOWN, REFSEL=11)
                             (VBUS wake)              ISR вооружён
                                      │                       │
                                      ▼               [устройство работает]
                             enterSystemOff()                  │
                                      │               батарея садится < ~3.0 В
                                      │                       │
                             [SYSTEMOFF]              LPCOMP DOWN → ISR
                                      │                       │
                             ┌────────┴────────┐     USB И V_bat ≥ 3000 мВ?
                             │                 │             │
                         V_bat ↑ ~3.9 В    VBUS          Да │ Нет
                         LPCOMP UP         подключён        │    │
                             │                 │         [пропуск] │
                             └────────┬────────┘              ▼
                                      │             initiateShutdown(LOW_VOLTAGE)
                                      ▼                       │
                                   [reset]          configureVoltageWake()
                                      │             (LPCOMP UP, REFSEL=12)
                                      └──────────── (VBUS wake)
                                                              │
                                                    enterSystemOff()
                                                              │
                                                         [SYSTEMOFF]
```

### Boot protection (АЦП)

- Запускается в `begin()` до включения питания радиомодуля
- Использует `getBattMilliVolts()` с внутренним опорным 0.6 В — абсолютное измерение, не зависит от VDD
- Пропускается если `isExternalPowered()` = true (VBUS детектирован)
- При срабатывании: `initiateShutdown(BOOT_PROTECT)` → `configureVoltageWake()` → `enterSystemOff()`

### Runtime low-voltage shutdown (LPCOMP DOWN)

- `configureLowVoltageAlert(AIN=7, REFSEL=11)` вызывается в `begin()` после `checkBootVoltage()`
- LPCOMP настроен на DOWN-событие (нисходящее пересечение порога)
- ISR `LPCOMP_COMP_IRQHandler`: проверяет `EVENTS_DOWN`, вызывает `lpcompDownHandler()`
- `lpcompDownHandler()`: shutdown, если `!isExternalPowered()` **или** `getBattMilliVolts() < 3000`
- При USB подключённом и V_bat ≥ 3000 мВ: shutdown **не происходит** — устройство работает на зарядке
- При USB подключённом но V_bat < 3000 мВ: shutdown **происходит** — зарядный ток недостаточен для поддержания нагрузки, shutdown предотвращает brownout-цикл

### Voltage-recovery wake (LPCOMP UP + VBUS)

Вызывается из `initiateShutdown()` при причинах `LOW_VOLTAGE` и `BOOT_PROTECT`:

- `configureVoltageWake(AIN=7, REFSEL=12)` — настраивает LPCOMP на UP-событие (порог ~3.9 В)
- Параллельно включается VBUS-пробуждение (`sd_power_usbdetected_enable` / `POWER_INTENSET_USBDETECTED`)
- В SYSTEMOFF: устройство просыпается от любого из двух событий:
  - V_bat поднялась выше ~3.9 В (LPCOMP UP) — например при зарядке от солнечной панели через USB
  - VBUS появился (USB подключён) — встроенный зарядник платы

### Причины выключения (GPREGRET2)

| Код | Константа | Описание |
|-----|-----------|----------|
| 0x00 | `SHUTDOWN_REASON_NONE` | Обычная загрузка |
| 0x4C | `SHUTDOWN_REASON_LOW_VOLTAGE` | Runtime разряд ниже ~3.0 В |
| 0x55 | `SHUTDOWN_REASON_USER` | Ручной `powerOff()` |
| 0x42 | `SHUTDOWN_REASON_BOOT_PROTECT` | Boot protection при старте |

## Поведение платы

### `promicro` (Nice!nano / ProMicro nRF52840)

- `initiateShutdown()`: отключает `SX126X_POWER_EN` перед переходом в SYSTEMOFF
- Порядок в `begin()`: init → `checkBootVoltage()` → `configureLowVoltageAlert()` → `SX126X_POWER_EN = HIGH`

Ручной `powerOff()` на плате не изменён — выполняет чистый `sd_power_system_off()` без вооружения LPCOMP (пробуждение только по кнопке reset или VBUS).

## CLI

| Команда | Результат |
|---------|-----------|
| `get pwrmgt.support` | `supported` |
| `get pwrmgt.source` | `battery` / `external` |
| `get pwrmgt.bootreason` | строки причины reset и shutdown |
| `get pwrmgt.bootmv` | напряжение батареи при старте (мВ) |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/helpers/NRF52Board.h` | `lpcomp_low_refsel` в `PowerMgtConfig`; `configureLowVoltageAlert()`, `s_power_instance`, `lpcompDownHandler()` |
| `src/helpers/NRF52Board.cpp` | Реализация `configureLowVoltageAlert()`; ISR `LPCOMP_COMP_IRQHandler`; статические определения |
| `variants/promicro/variant.h` | `PWRMGT_LPCOMP_REFSEL=12`, `PWRMGT_LPCOMP_LOW_REFSEL=11`; исправлен баг (было 11, давало ~2.7 В — ниже boot lock) |
| `variants/promicro/PromicroBoard.cpp` | `power_config` + вызов `configureLowVoltageAlert()` |
