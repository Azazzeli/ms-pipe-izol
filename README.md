# ms-pipe-izol (MsPipeIzol)

Плагин nanoCAD / Model Studio: выравнивание труб и теплоизоляция.

## Команды

- `PIPE_ALIGN`
- `PIPE_INSUL` — одна ось
- `PIPE_INSUL_ALL` — все оси / зоны

## Сборка

`MsPipeIzol.sln` → Release NCAD x64  
Готовый модуль: `deploy/MsPipeIzol.nrx`

## Изоляция (новый способ)

3D-объект поверх трубы — через `CInsulModel::create_insul`  
(как команда MS `PIPE_MULTIPIPE_INSULATION`).

Подробно: [docs/06_CINSULMODEL_CREATE_INSUL.md](docs/06_CINSULMODEL_CREATE_INSUL.md)
