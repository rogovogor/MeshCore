# Конвенция: Версия прошивки с git-коммитом

## Правило

Строка версии прошивки **всегда включает короткий хеш git-коммита**, по которому была сделана сборка:

```
v1.14.1-abc1234
```

Это позволяет точно определить, какой код прошит в устройство, без дополнительных записей.

---

## Реализация

### `git_version.py` (корень репозитория)

Pre-build скрипт PlatformIO. Запускается перед каждой сборкой, инжектирует хеш коммита и дату билда:

```python
commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode().strip()
env.Append(CPPDEFINES=[("GIT_COMMIT", '\\"' + commit + '\\"')])

date_str = datetime.now().strftime("%d %b %Y")
env.Append(CPPDEFINES=[("FIRMWARE_BUILD_DATE", '\\"' + date_str + '\\"')])
```

Результат: `-D GIT_COMMIT="abc1234"` и `-D FIRMWARE_BUILD_DATE="20 Apr 2026"` передаются компилятору при любой сборке — и через `build.sh`, и через прямой `pio run`.

### `platformio.ini`

Скрипт подключён к каждой базовой платформе:

| Base | extra_scripts |
|---|---|
| `arduino_base` | `pre:git_version.py` |
| `esp32_base` | `pre:git_version.py`, `merge-bin.py` |
| `nrf52_base` | `pre:git_version.py`, `create-uf2.py`, `patch_bluefruit.py` |
| `stm32_base` | `pre:git_version.py`, `arch/stm32/build_hex.py` |
| `rp2040_base` | наследует из `arduino_base` |

### `examples/companion_radio/MyMesh.h`

```cpp
#ifndef FIRMWARE_VERSION
  #ifdef GIT_COMMIT
    #define FIRMWARE_VERSION "v1.15.0-" GIT_COMMIT
  #else
    #define FIRMWARE_VERSION "v1.15.0"
  #endif
#endif
```

C конкатенирует соседние строковые литералы: `"v1.15.0-" "abc1234"` → `"v1.15.0-abc1234"`.

> При обновлении upstream версии — менять только числовую часть (`v1.15.0`). Хеш подставляется автоматически.

---

## Отображение на сплеш-экране (`UITask.cpp`)

`FIRMWARE_VERSION = "v1.15.1b2-abc1234"` разбивается по первому `-`:

| Элемент | Значение | Размер шрифта | Y |
|---------|----------|---------------|---|
| Версия | `v1.15.1b2` | 2 (крупный) | 22 |
| Коммит | `#abc1234` | 1 (мелкий) | 41 |
| Дата | `20 Apr 2026` | 1 (мелкий) | 51 |

> Буфер `_version_info` — 12 байт. Максимум 11 символов до `-`. `v1.15.1b10` = 10 символов — влезает.

---

## Схема версионирования для кастомных сборок

Upstream выпускает `v1.15.1`, кастомные сборки поверх него:

```
v1.15.1b1  — первая кастомная сборка на базе v1.15.1
v1.15.1b2  — вторая, и т.д.
```

GIT-тег для GHA: `companion-v1.15.1b1` → `GIT_TAG_VERSION=v1.15.1b1` → файлы `...-v1.15.1b1-abc1234.uf2`.

---

## Правило при обновлении версии

При смене upstream версии — обновить числовую часть в `MyMesh.h` (`v1.15.0` → `v1.15.1`).
Хеш и дата подставляются автоматически при сборке.

**Не нужно** вручную вписывать хеш или дату.
