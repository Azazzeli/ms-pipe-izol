# Сборка MsPipeIzol

Актуальная инструкция — в корневом [README.md](../README.md). Кратко:

## Пути

Отредактируйте [`MsPipeIzol.paths.props`](../MsPipeIzol.paths.props):

- `NCadSdkRoot` — nanoCAD SDK  
- `ModelStudioRoot` — исходники Model Studio  
- `MsLibConfig` — например `Release NCAD241`

Шаблон: [`MsPipeIzol.paths.props.example`](../MsPipeIzol.paths.props.example).

## Команда

```bat
msbuild MsPipeIzol.sln /t:Rebuild /p:Configuration="Release NCAD" /p:Platform=x64 /p:PlatformToolset=v143
```

Результат: `bin-x64\Release NCAD\MsPipeIzol.nrx` и копия в `deploy\` (PostBuild).

## MFC

Нужен компонент Visual Studio **MFC** (Individual components). Без него: `MSB8041`.

## Кодировка

- `.cpp` — литералы `\xXXXX`  
- `.rc` — Windows-1251  
- не включать `/utf-8` на весь проект
