# Builds the native port with MinGW-w64 + CMake.
#
#   .\build.ps1                          # uses the local SDL2 under work\toolchain
#   .\build.ps1 -SdlDir C:\SDL2\x86_64-w64-mingw32
#   .\build.ps1 -Download                # fetches SDL2-devel-<version>-mingw.zip
#
# The SDL2 DLL is copied next to buraco.exe because the POST_BUILD step in
# CMakeLists.txt only copies assets/.
param(
    [string]$SdlDir,
    [string]$BuildDir = "$PSScriptRoot\build",
    [string]$Version = "2.32.10",
    [switch]$Download
)

$ErrorActionPreference = "Stop"

function Find-Sdl([string]$root, [string]$version) {
    $bases = @(
        (Join-Path $root "..\work\toolchain\sdl2"),
        (Join-Path $root "toolchain\sdl2")
    )
    foreach ($base in $bases) {
        $direct = Join-Path $base "SDL2-$version\x86_64-w64-mingw32"
        if (Test-Path (Join-Path $direct "lib\cmake\SDL2\sdl2-config.cmake")) { return $direct }
        $hit = Get-ChildItem -Path $base -Filter sdl2-config.cmake -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like "*x86_64-w64-mingw32*" } |
            Select-Object -First 1
        if ($hit) { return (Split-Path (Split-Path (Split-Path $hit.FullName -Parent) -Parent) -Parent) }
    }
    return $null
}

if (-not $SdlDir) {
    $SdlDir = Find-Sdl $PSScriptRoot $Version
}

if (-not $SdlDir) {
    if (-not $Download) {
        throw "SDL2 nao encontrado. Passe -SdlDir <pasta x86_64-w64-mingw32> ou use -Download."
    }
    $zip = Join-Path $PSScriptRoot "..\work\toolchain\SDL2-devel-$Version-mingw.zip"
    New-Item -ItemType Directory -Force (Split-Path $zip) | Out-Null
    Write-Host "Baixando SDL2 $Version..."
    Invoke-WebRequest "https://github.com/libsdl-org/SDL/releases/download/release-$Version/SDL2-devel-$Version-mingw.zip" -OutFile $zip
    Expand-Archive $zip -DestinationPath (Join-Path $PSScriptRoot "..\work\toolchain\sdl2") -Force
    $SdlDir = Join-Path $PSScriptRoot "..\work\toolchain\sdl2\SDL2-$Version\x86_64-w64-mingw32"
}

if (-not (Test-Path (Join-Path $SdlDir "lib\cmake\SDL2\sdl2-config.cmake"))) {
    throw "SDL2 invalido em '$SdlDir' (esperado lib\cmake\SDL2\sdl2-config.cmake)."
}

Write-Host "SDL2: $SdlDir"
cmake -S $PSScriptRoot -B $BuildDir -G "MinGW Makefiles" `
      -DCMAKE_BUILD_TYPE=Release "-DSDL2_DIR=$SdlDir\lib\cmake\SDL2"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

Copy-Item (Join-Path $SdlDir "bin\SDL2.dll") $BuildDir -Force
Write-Host "OK: $(Join-Path $BuildDir 'buraco.exe')"

