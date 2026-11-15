#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from pathlib import Path

SDK_ROOT = r"D:\WORK\SAP\Model Studio"
PROJECT_DIR = r"D:\WORK\SAP\HelloNRX"
OUTPUT_PROPS = os.path.join(PROJECT_DIR, "viper_sdk_auto.props")

def find_include_dirs(root: str) -> list:
    """Ищет "публичные" include-пути: все папки Source\*, но НЕ исходники (src, impl, private)."""
    root_path = Path(root)
    inc_dirs = set()

    # Основные папки с интерфейсами
    source_dir = root_path / "Source"
    if source_dir.is_dir():
        for item in source_dir.iterdir():
            if item.is_dir():
                name = item.name.lower()
                # Пропускаем внутренние/исходные папки
                if any(skip in name for skip in ["test", "sample", "private", "impl", "internal"]):
                    continue
                # Берём папку, если в ней есть .h файлы
                if list(item.rglob("*.h")):
                    inc_dirs.add(str(item))
                # Также ищем подпапки первого уровня (часто inc/, include/)
                for sub in item.iterdir():
                    if sub.is_dir() and sub.name.lower() in ["inc", "include", "api", "public", "headers"]:
                        if list(sub.rglob("*.h")):
                            inc_dirs.add(str(sub))

    return sorted(inc_dirs)

def find_lib_files(root: str) -> tuple:
    """Ищет .lib файлы и возвращает (список lib-путей, список имён lib)."""
    root_path = Path(root)
    lib_dirs = set()
    lib_names = set()

    # Ищем везде, где есть 'lib', 'x64', 'bin'
    for item in root_path.rglob("*.lib"):
        # Пропускаем debug-версии, если не нужны
        if "debug" in str(item).lower() and "release" not in str(item).lower():
            continue
        lib_dirs.add(str(item.parent))
        lib_names.add(item.name)

    return sorted(lib_dirs), sorted(lib_names)

def main():
    print("🔍 Авто-поиск SDK Модель Студия...")
    print(f"   Корень: {SDK_ROOT}")

    # 1. Include-пути
    inc_dirs = find_include_dirs(SDK_ROOT)
    print(f"\n✅ Найдено include-папок: {len(inc_dirs)}")
    for d in inc_dirs[:5]:
        print(f"   → {d}")
    if len(inc_dirs) > 5:
        print(f"   ... (+{len(inc_dirs)-5})")

    # 2. Lib-файлы
    lib_dirs, lib_names = find_lib_files(SDK_ROOT)
    print(f"\n✅ Найдено .lib-файлов: {len(lib_names)}")
    for name in lib_names[:8]:
        print(f"   → {name}")
    if len(lib_names) > 8:
        print(f"   ... (+{len(lib_names)-8})")

    # 3. Формируем .props
    inc_str = ";\n        ".join(inc_dirs)
    libdir_str = ";\n        ".join(lib_dirs)
    lib_str = ";".join(lib_names)

    props_content = f"""<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="UserMacros">
    <ViperSdkRoot>{SDK_ROOT}</ViperSdkRoot>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>
        {inc_str};
        %(AdditionalIncludeDirectories)
      </AdditionalIncludeDirectories>
      <PreprocessorDefinitions>
        VIPERCS_MODULE;
        _WINDOWS;
        _USRDLL;
        ACRX_DECLARE_GX;
        %(PreprocessorDefinitions)
      </PreprocessorDefinitions>
      <LanguageStandard>stdcpp17</LanguageStandard>
      <RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>
      <AdditionalOptions>/Zc:__cplusplus %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>

    <Link>
      <AdditionalLibraryDirectories>
        {libdir_str};
        %(AdditionalLibraryDirectories)
      </AdditionalLibraryDirectories>
      <AdditionalDependencies>
        {lib_str};
        accore.lib;
        acdb.lib;
        acge.lib;
        rxapi.lib;
        %(AdditionalDependencies)
      </AdditionalDependencies>
      <ModuleDefinitionFile>HelloNRX.def</ModuleDefinitionFile>
    </Link>
  </ItemDefinitionGroup>
</Project>
"""

    # Сохраняем
    with open(OUTPUT_PROPS, "w", encoding="utf-8") as f:
        f.write(props_content)

    print(f"\n🎯 Успешно создано: {OUTPUT_PROPS}")
    print("\n📌 Далее:")
    print("   1. В Visual Studio: Вид → Диспетчер свойств")
    print("   2. ПКМ по (Debug | x64) → Добавить → Существующий лист свойств...")
    print(f"   3. Выберите: {OUTPUT_PROPS}")
    print("   4. Убедитесь, что есть HelloNRX.def (см. ниже!)")

    # Создаём .def файл, если нет
    def_file = os.path.join(PROJECT_DIR, "HelloNRX.def")
    if not os.path.exists(def_file):
        with open(def_file, "w", encoding="utf-8") as f:
            f.write("""LIBRARY HelloNRX
EXPORTS
acrxGetApiVersion
acrxEntryPoint
""")
        print(f"   ✅ Создан: HelloNRX.def (обязателен для NRX!)")

if __name__ == "__main__":
    main()