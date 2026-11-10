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
    [string]$Sha256 = "",
    [switch]$Download
)

$ErrorActionPreference = "Stop"

# SHA-256 of the official SDL2-devel-<version>-mingw.zip published on GitHub.
# Add new entries only after verifying the file against the official release.
$KnownSha256 = @{
    "2.32.10" = "F15CFF5FCA62EC9381A016EF1D42A95C638CD72D2F226BA5781C76FE43DBD1AC"
}

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
    if (-not $Sha256) { $Sha256 = $KnownSha256[$Version] }
    if (-not $Sha256) {
        throw "sem SHA-256 conhecido para SDL2 $Version. Verifique o arquivo oficial e passe -Sha256 <hash>."
    }
    $expected = $Sha256.Trim().ToUpperInvariant()
    New-Item -ItemType Directory -Force (Split-Path $zip) | Out-Null

    # Reuse a previously verified package; otherwise download and check it
    # before extracting anything.
    $verified = $false
    if (Test-Path $zip) {
        $cached = (Get-FileHash $zip -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($cached -eq $expected) {
            Write-Host "SHA-256 OK (arquivo existente): $cached"
            $verified = $true
        }
    }
    if (-not $verified) {
        Write-Host "Baixando SDL2 $Version..."
        Invoke-WebRequest "https://github.com/libsdl-org/SDL/releases/download/release-$Version/SDL2-devel-$Version-mingw.zip" -OutFile $zip
        $actual = (Get-FileHash $zip -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($actual -ne $expected) {
            Remove-Item $zip -Force -ErrorAction SilentlyContinue
            throw "checksum SHA-256 invalido para SDL2 $Version (esperado $expected, obtido $actual); download descartado."
        }
        Write-Host "SHA-256 OK: $actual"
    }
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

