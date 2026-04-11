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

Pre-build скрипт PlatformIO. Запускается перед каждой сборкой, получает хеш текущего коммита и передаёт его как C define:

```python
commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode().strip()
env.Append(CPPDEFINES=[("GIT_COMMIT", '\\"' + commit + '\\"')])
```

Результат: `-D GIT_COMMIT="abc1234"` передаётся компилятору.

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
    #define FIRMWARE_VERSION "v1.14.1-" GIT_COMMIT
  #else
    #define FIRMWARE_VERSION "v1.14.1"
  #endif
#endif
```

C конкатенирует соседние строковые литералы: `"v1.14.1-" "abc1234"` → `"v1.14.1-abc1234"`.

---

## Отображение

- **Companion app**: отображает полную строку `v1.14.1-abc1234`
- **Дисплей устройства** (`UITask.cpp`): стрипает хеш по `-` → показывает `v1.14.1`
  (код уже был рассчитан на этот формат)

---

## Правило при обновлении версии

При смене `FIRMWARE_VERSION` в `MyMesh.h` — обновлять только числовую часть (`v1.14.1`).
Хеш подставляется автоматически при сборке.

**Не нужно** вручную вписывать хеш в `FIRMWARE_VERSION`.
