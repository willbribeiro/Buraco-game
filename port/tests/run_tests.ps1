# Compila o porte e roda os testes unitários.
#
#   .\run_tests.ps1                 # build + suíte completa
#   .\run_tests.ps1 canastra        # só casos cujo nome contém "canastra"
#
# Requisitos: os mesmos do build.ps1 (MinGW-w64 + CMake + SDL2).
param(
    [string]$Filter,
    [string]$BuildDir = "$PSScriptRoot\..\build"
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\..\build.ps1" -BuildDir $BuildDir

$exe = Join-Path $BuildDir "tests\buraco_tests.exe"
if (-not (Test-Path $exe)) {
    throw "Test executable not found: $exe"
}

if ($Filter) {
    & $exe $Filter
} else {
    & $exe
}
exit $LASTEXITCODE
