# Сборка и загрузка

## Требования

- Visual Studio 2022 (MSBuild 17.x)
- SDK nanoCAD (пути в `viper_sdk_auto.props` / `all_folders.props`)
- Заголовки/библиотеки Model Studio (`ViperCSObj.lib` и include)

## Сборка

Из корня исходников (`src/` или `D:\WORK\SAP\HelloNRX`):

```bat
msbuild HelloNRX.sln /t:Rebuild /p:Configuration="Release NCAD" /p:Platform=x64
```

Выход:

```
bin-x64\Release NCAD\HelloNRX.nrx
```

Копия в пакет:

```
deploy\HelloNRX.nrx
deploy\HelloNRX_izol8.nrx
```

## Генерация props (если сменились пути SDK)

```powershell
.\Generate-ModelStudioProps.ps1
# или
python generate_modelstudio_props.py
```

## Загрузка в nanoCAD

1. Открыть чертёж Model Studio (трубопроводы).
2. `APPLOAD` → `HelloNRX_izol8.nrx`
3. Проверить сообщения:
   - `HelloNRX: PIPE_ALIGN, PIPE_INSUL, PIPE_INSUL_ALL`
4. Выгрузка: `APPUNLOAD` того же файла.

## Важно про кодировку

Строки UI в `.cpp`/`.rc` должны быть в **CP1251**, не UTF-8 без BOM — иначе «кракозябры» в диалоге.
