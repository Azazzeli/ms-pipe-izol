# MsPipeIzol

Плагин **nanoCAD + Model Studio CS**: расчёт и назначение теплоизоляции трубопроводов по **НР 34-70-118-87** (помещение).

| | |
|---|---|
| Модуль | `deploy/MsPipeIzol.nrx` |
| Команды | `PIPE_INSUL`, `PIPE_INSUL_ALL` |
| UI | модальный MFC-диалог (Segoe UI) |
| 3D | `CInsulModel::create_insul` / `create_insul_elbow` |

---

## Быстрый старт (готовая сборка)

1. Полностью закройте nanoCAD.
2. `APPLOAD` → [`deploy/MsPipeIzol.nrx`](deploy/MsPipeIzol.nrx).
3. В чертеже Model Studio (трубопроводы):
   - **`PIPE_INSUL`** — выбрать трубу/ось → расчёт → **Назначить**
   - **`PIPE_INSUL_ALL`** — все оси / зоны (в т.ч. разрез по сварным швам)
4. Выгрузка: `APPUNLOAD` того же файла (окно команды к этому моменту уже закрыто).

В консоли при назначении смотрите строки `[PIPE_INSUL] create_insul …` / `create_insul_elbow …`.

---

## Сборка из исходников

### Требования

- **Visual Studio 2022** (toolset `v143`) + компонент **MFC**
- **nanoCAD SDK** (nrx / arxgate)
- **Исходники / libs Model Studio** (`ViperCSObj.lib`, заголовки `ViperCSObj`, …)

### 1. Укажите пути

Откройте [`MsPipeIzol.paths.props`](MsPipeIzol.paths.props) и замените:

| Свойство | Что это |
|----------|---------|
| `NCadSdkRoot` | корень nanoCAD SDK (`include`, `include-x64`, `lib-x64`) |
| `ModelStudioRoot` | корень Model Studio (внутри есть `Source\`, `Common\`, `API\`) |
| `MsLibConfig` | папка libs, напр. `Release NCAD241` → `Source\lib\VS16\Release NCAD241\x64` |

Шаблон с явными `C:\Path\To\…`: [`MsPipeIzol.paths.props.example`](MsPipeIzol.paths.props.example).

По умолчанию props ожидает соседние папки:

```text
…\SAP\
    ISUL\          ← этот репозиторий
    ncadsdk\
    Model Studio\
```

### 2. Соберите

**Visual Studio:** `MsPipeIzol.sln` → конфигурация **`Release NCAD | x64`** → Build.

**Или из консоли** (Developer PowerShell / VS2022):

```bat
msbuild MsPipeIzol.sln /t:Rebuild /p:Configuration="Release NCAD" /p:Platform=x64 /p:PlatformToolset=v143
```

Выход:

```text
bin-x64\Release NCAD\MsPipeIzol.nrx
```

Скопируйте в пакет:

```bat
copy /Y "bin-x64\Release NCAD\MsPipeIzol.nrx" deploy\MsPipeIzol.nrx
```

### 3. Важно про кодировку

- Строки в `.cpp` — wide-литералы с `\xXXXX` (ASCII-safe).
- `HelloNRX.rc` — **Windows-1251** + `#pragma code_page(1251)`.
- **Не** включайте `/utf-8` для всего проекта: заголовки SDK часто в CP1251.

---

## Что делает плагин

- Расчёт δ по **НР 34**, всегда **помещение** (`K=1`, `tвозд=25 °C`) — регион в UI не нужен.
- Параметры на осевую **не пишутся** — только 3D-объект изоляции.
- Трубы: `CInsulModel::create_insul`.
- Отводы: `create_insul_elbow` (по одному), при необходимости fallback `vCSInsulation` / `MultiPipeInsulation`.

Подробнее: [docs/](docs/).

---

## Структура репозитория

```text
MsPipeIzol.sln                 решение
MsPipeIzol.vcxproj             проект NRX
MsPipeIzol.paths.props         <<< пути SDK (редактировать)
MsPipeIzol.paths.props.example шаблон путей
HelloNRX.cpp                   команды + entry point
InsulDlg.cpp / .h / .rc        диалог расчёта / назначения
InsulCalc.cpp / .h             движок НР 34 / СП 61
deploy/MsPipeIzol.nrx          готовый модуль
docs/                          описание API и сборки
```

---

## Лицензия / использование

Внутренний инструмент для контура Model Studio / nanoCAD. Подставьте свои пути SDK и собирайте под вашу версию nanoCAD (24.x и libs `NCAD24` / `NCAD241`).
