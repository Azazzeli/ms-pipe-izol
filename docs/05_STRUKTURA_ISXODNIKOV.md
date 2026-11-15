# Структура исходников

```
src/
  HelloNRX.sln / .vcxproj     — решение VS
  HelloNRX.cpp                — команды, entry point
  HelloNRX.def / .rc          — экспорты, ресурсы диалогов
  resource.h                  — ID контролов
  stdafx.h                    — PCH
  AlignDlg.h/.cpp             — PIPE_ALIGN
  InsulDlg.h/.cpp             — PIPE_INSUL / PIPE_INSUL_ALL
  InsulCalc.h/.cpp            — расчёт толщины
  viper_sdk_auto.props        — include/lib Model Studio + nanoCAD
  all_folders.props           — дополнительные пути
  generate_modelstudio_props.py
  Generate-ModelStudioProps.ps1
```

## Ключевые типы в InsulDlg

- `InsulDlgMode::SingleAxis` / `AllAxes` — режим команды
- `InsulZone` — строка списка (ось, зона, трубы, t, среда, δ)
- `ShowModeless` — Create + ShowWindow, один экземпляр `s_pInstance`
- `HighlightAxis` / `HighlightPipes` / `ClearHighlight` — `acedSSSetFirst`
- `RebuildZones` / `BuildZonesForAxis` / `CollectAllAxes`
- `AssignToPipes` / `WriteAxisParams` — запись в модель

## Рабочая папка разработки

`D:\WORK\SAP\HelloNRX` — полный проект со сборкой:

- `bin-x64\` — выход NRX
- `trash-x64\` — obj/tlog
- `HelloNRX_izol*.nrx` — снимки сборок в корне
