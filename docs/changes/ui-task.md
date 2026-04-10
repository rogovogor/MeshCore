# UITask.cpp — адаптивный UI и MsgPreview

**Файл:** `examples/companion_radio/ui-new/UITask.cpp`

## Вектор изменений

Два независимых направления: (1) адаптивная раскладка HomeScreen под дисплеи разной ширины, (2) переработка экрана предпросмотра непрочитанных сообщений.

---

## 1. Адаптивная раскладка HomeScreen

Все страницы HomeScreen (`MSGS`, `RECENT`, `RADIO`, `BLUETOOTH`, `ADVERT`, `GPS`, `SENSORS`, `SHUTDOWN`) переведены с хардкод-координат на вычисляемые метрики:

```cpp
int hdr_size   = (display.width() >= 200) ? 2 : 1;  // размер шрифта заголовка
int hdr_line_h = 8 * hdr_size;                        // высота строки заголовка
int dot_y      = hdr_line_h + 5;                      // Y индикатора страницы
int content_y  = hdr_line_h + 14;                     // Y начала контента
int line_h     = 8 * hdr_size + 3;                    // межстрочный интервал
```

- **Узкие дисплеи** (< 200 px): `hdr_size=1`, поведение как прежде.
- **Широкие дисплеи** (≥ 200 px): `hdr_size=2`, крупный шрифт в заголовке и контенте.
- Страница `RADIO`: для `hdr_size=2` используются укороченные форматы строк (`FQ:%.3f SF:%d` вместо `FQ: %06.3f   SF: %d`).
- Все элементы позиционируются относительно `content_y` и `display.height()` вместо абсолютных `y=18`, `y=64-11` и т.д.

### Удаление showAlert

При навигации на страницу `RECENT` убран вызов `_task->showAlert("Recent adverts", 800)` — был лишним.

---

## 2. MsgPreview — переработка структуры и рендера

### Структура `MsgEntry`

До:
```cpp
char origin[62];   // форматированная строка "(D) name:" или "(N) name:"
char msg[78];
```
После:
```cpp
uint8_t  path_len;        // 0xFF = direct, иначе hop count
char     from_name[32];   // имя отправителя / канала
char     msg[MAX_TEXT_LEN];
```
Данные хранятся неформатированными, форматирование перенесено в `render()`.

### `addPreview()`

Убрано `sprintf` для `origin`; вместо него — прямое копирование `path_len` и `from_name` через `strncpy` с NUL-гарантией.

### `render()` — новый алгоритм

1. **Метрики**: `hdr_size` по ширине дисплея, `msg_start_y = hdr_line_h + 3`.
2. **Выбор размера шрифта тела**: симулируется перенос по словам при `size=2`; если сообщение умещается — используется `body_size=2`, иначе `body_size=1`.
3. **Заголовок**: `#N (D)name` (direct) или `#N [hops]name` (group) слева + время `Xs/Xm/Xh` справа, оба в `hdr_size`.
4. **Разделитель**: горизонтальная линия на `hdr_line_h + 2`.
5. **Тело**: `printWordWrap()` в `body_size`.
6. Убрана отрисовка `"Unread: N"` и старый layout с `origin`.
