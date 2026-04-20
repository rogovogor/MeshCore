# eInk display rotation build flag support

**Файлы:** `src/helpers/ui/E213Display.cpp`, `src/helpers/ui/E290Display.cpp`

Добавлена поддержка макроса `DISPLAY_ROTATION` для e-ink драйверов `E213Display` и `E290Display`.

## Что изменено

- В `E213Display.cpp` и `E290Display.cpp` добавлена дефолтная константа:
  - `#ifndef DISPLAY_ROTATION`
  - `#define DISPLAY_ROTATION 3`
- Вместо жёсткой установки `display->setRotation(3)` теперь используется `display->setRotation(DISPLAY_ROTATION)`.

## Почему это важно

- Ориентацию экрана можно задавать через build-флаг `-D DISPLAY_ROTATION=<0|1|2|3>`.
- Устройства с e-ink дисплеем теперь могут компилироваться с требуемым поворотом без правки исходников.
- `heltec_wireless_paper`, использующий `E213Display`, также получает эту настройку.

## Поведение по умолчанию

Если `DISPLAY_ROTATION` не задан, используется значение `3` (ландшафт, поворот на 180°).