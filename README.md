# MsPipeIzol

Плагин **nanoCAD + Model Studio CS Трубопроводы**: выравнивание осей и расчёт/назначение теплоизоляции.

Раньше: HelloNRX. Сейчас проект называется **MsPipeIzol** (репозиторий `ms-pipe-izol`).

## Команды

| Команда | Описание |
|---------|----------|
| `PIPE_ALIGN` | Выравнивание узлов оси (модальный диалог) |
| `PIPE_INSUL` | Изоляция одной оси (выбор + modeless + подсветка) |
| `PIPE_INSUL_ALL` | Все оси / зоны по Weld (modeless + подсветка) |

## Сборка

- VS 2022, конфигурация **Release NCAD**, x64
- Решение: `MsPipeIzol.sln`
- Выход: `bin-x64\Release NCAD\MsPipeIzol.nrx` (или как задано в проекте)

Загрузка в nanoCAD: `APPLOAD` → NRX из `deploy\` или `bin-x64\`.

## Документация

См. папку `docs\`:

- `00_OBZOR.md` — обзор
- `01_KOMANDY.md` — команды
- `02_IZOLYACIYA.md` — нормы и расчёт
- `03_SBORKA.md` — сборка
- `04_API_OSI.md` — API осей Model Studio
- `05_STRUKTURA_ISXODNIKOV.md` — структура

Один файл со всем C++: `HelloNRX_ALL.cpp` (amalgamation).

## Зависимости

nanoCAD NRX SDK, Model Studio (`ViperCSObj`: `vCSAxis`, сегменты, Weld, параметры).
