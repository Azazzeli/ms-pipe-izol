# ms-pipe-izol (MsPipeIzol)

Плагин nanoCAD / Model Studio: теплоизоляция труб.

## Команды

- `PIPE_INSUL` — одна ось
- `PIPE_INSUL_ALL` — все оси / зоны

## Сборка

`MsPipeIzol.sln` → Release NCAD x64  
Готовый модуль: `deploy/MsPipeIzol.nrx`

## Изоляция

3D-объект: `CInsulModel::create_insul` (как `PIPE_MULTIPIPE_INSULATION`).

См. `docs/06_CINSULMODEL_CREATE_INSUL.md`
