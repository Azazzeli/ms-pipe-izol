# Generate-ModelStudioProps.ps1
# Автоматически находит include/lib пути в D:\WORK\SAP\Model Studio

$SdkRoot = "D:\WORK\SAP\Model Studio"
$OutputProps = "$(Split-Path $MyInvocation.MyCommand.Path)\modelstudio.props"

# === 1. Ищем ВСЕ папки include/inc ===
$includeDirs = @()
Write-Host "🔍 Поиск include/inc в: $SdkRoot" -ForegroundColor Cyan
Get-ChildItem -Path $SdkRoot -Recurse -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^(inc|include)$' -and $_.FullName -notlike '*\test*' -and $_.FullName -notlike '*\samples*' } |
    ForEach-Object {
        $includeDirs += $_.FullName
        Write-Host "  + $($_.FullName)" -ForegroundColor DarkGray
    }

# Убираем дубли (case-insensitive)
$includeDirs = $includeDirs | Sort-Object | Get-Unique

# === 2. Ищем ВСЕ папки lib (x64) ===
$libDirs = @()
Write-Host "`n🔍 Поиск lib (x64/x86) в: $SdkRoot" -ForegroundColor Cyan
Get-ChildItem -Path $SdkRoot -Recurse -Directory -ErrorAction SilentlyContinue |
    Where-Object {
        ($_.Name -eq "lib" -or $_.Name -eq "Lib") -and
        ($_.Parent.Name -eq "x64" -or $_.Parent.Name -eq "x86" -or
         $_.Name -like "*x64*" -or $_.Name -like "*64*" -or
         $_.FullName -notlike '*\debug\*' -and $_.FullName -notlike '*\release\*')
    } |
    ForEach-Object {
        $libDirs += $_.FullName
        Write-Host "  + $($_.FullName)" -ForegroundColor DarkGray
    }

$libDirs = $libDirs | Sort-Object | Get-Unique

# === 3. Формируем .props ===
$propsContent = @"
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="UserMacros">
    <ModelStudioSdkRoot>$SdkRoot</ModelStudioSdkRoot>
    <ModelStudioIncludeDirs>$($includeDirs -join ';')</ModelStudioIncludeDirs>
    <ModelStudioLibraryDirs>$($libDirs -join ';')</ModelStudioLibraryDirs>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>$(ModelStudioIncludeDirs);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <AdditionalLibraryDirectories>$(ModelStudioLibraryDirs);%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    </Link>
  </ItemDefinitionGroup>
</Project>
"@

# Сохраняем
Set-Content -Path $OutputProps -Value $propsContent -Encoding UTF8
Write-Host "`n✅ Успешно создано: $OutputProps" -ForegroundColor Green
Write-Host "   Include путей: $($includeDirs.Count)"
Write-Host "   Library путей: $($libDirs.Count)"
Write-Host "`n📌 Теперь:"
Write-Host "   1. В Visual Studio: ПКМ по проекту → Add → Existing Item → выберите modelstudio.props"
Write-Host "   2. Пересоберите проект — всё должно подключиться!"