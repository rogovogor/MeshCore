# Changes vs upstream (branch: Cyrillic_supp4all → main)

Документация изменений текущей ветки относительно `main`.

| Файл | Область | Краткое описание |
|------|---------|-----------------|
| [cyrillic-support.md](cyrillic-support.md) | `src/helpers/ui/`, `variants/*/platformio.ini` | Поддержка кириллицы через `-D CYRILLIC_SUPPORT=1` для всех GFX-дисплеев |
| [ui-task.md](ui-task.md) | `examples/companion_radio/ui-new/UITask.cpp` | Адаптивная раскладка HomeScreen + переработка MsgPreview |
| [heltec-wireless-paper.md](heltec-wireless-paper.md) | `variants/heltec_wireless_paper/` | Новый `WirelessPaperBoard` с корректным измерением батареи |
| [heltec-v4-lorafem.md](heltec-v4-lorafem.md) | `variants/heltec_v4/` | Класс `LoRaFEMControl` — авто-детекция GC1109/KCT8103L (V4.2/V4.3) |
| [build.md](build.md) | `build.sh`, `bin/uf2conv/uf2conv.py`, `git_version.py` | Суффиксы companion-сборок; фикс генерации .uf2 для nRF52; авто-дата билда |
| [eink-clock-page.md](eink-clock-page.md) | `UITask.cpp`, `UITask.h`, display drivers | Страница часов для eInk: крупные часы + inline-просмотр PM без смены экрана |
| [eink-display-rotation.md](eink-display-rotation.md) | `src/helpers/ui/E213Display.cpp`, `src/helpers/ui/E290Display.cpp` | Поддержка `DISPLAY_ROTATION` для eInk-драйверов E213/E290 |
| [firmware-version-convention.md](firmware-version-convention.md) | `git_version.py`, `platformio.ini`, `MyMesh.h` | Версия прошивки с git-хешем и датой; схема `v1.15.1b1` для кастомных сборок |
