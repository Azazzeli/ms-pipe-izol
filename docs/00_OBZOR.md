# Обзор проекта HelloNRX

## Назначение

NRX-модуль для nanoCAD Model Studio:

1. **PIPE_ALIGN** — выравнивание узлов оси трубопровода (углы поворота, ортогональность).
2. **PIPE_INSUL** — расчёт и назначение теплоизоляции **одной** выбранной оси (модельное окно, CAD не блокируется).
3. **PIPE_INSUL_ALL** — то же для **всех** осей модели: список зон, t/среда по зонам, разрез по сварным швам, подсветка выбранной оси.

## Модули исходников (`src/`)

| Файл | Роль |
|------|------|
| `HelloNRX.cpp` | Точка входа NRX, регистрация команд, resolve оси по клику |
| `AlignDlg.cpp/h` | Диалог выравнивания (модальный) |
| `InsulDlg.cpp/h` | Диалог изоляции (modeless), вкладки, зоны, подсветка, assign |
| `InsulCalc.cpp/h` | Расчёт δ по НР 34 / СП 61.13330 |
| `HelloNRX.rc` + `resource.h` | MFC-ресурсы диалогов |
| `stdafx.h` | Precompiled header, MFC/ARX |
| `*.props` | Пути к SDK Model Studio / nanoCAD |

## Зависимости SDK

- nanoCAD ObjectARX / NRX SDK
- Model Studio: `ViperCSObj` (`vCSAxis`, `vCSSegment`, `vCSSubSegment`, `vCSWeld`, …)
- `MultiPipeInsulation`, `ParamsObject`, `VCSUtils`, `vCSTools`

Сборка: конфигурация **Release NCAD**, платформа **x64**.

## Текущий статус (izol8)

| Функция | Статус |
|---------|--------|
| Расчёт δ (котлы/НР 34 + от ожога СП 61) | Работает |
| Назначение толщины на трубы/ось (PART_*) | Работает |
| Регионы РФ, t, среда на ось | Работает |
| Modeless диалоги (не блокируют чертёж) | Работает |
| Подсветка выбранной оси (`acedSSSetFirst`) | Работает |
| Список всех осей (`PIPE_INSUL_ALL`) | **Проблема: список может быть пуст** — см. `04_API_OSI.md` |
| PIPE_ALIGN | Работает (отдельный модальный диалог) |

## История сборок deploy

| Файл | Что внутри |
|------|------------|
| `HelloNRX_izol7.nrx` | GetAllAxis + зоны по Weld |
| `HelloNRX_izol8.nrx` | Две команды modeless + подсветка |
| `HelloNRX.nrx` | Копия последней Release NCAD |
