# Run Buraco Port Unit Tests
# Prerequisites: CMake, SDL2 library, Catch2 framework

$ErrorActionPreference = "Stop"

function Write-Header {
    param([string]$Title)
    Write-Host ""
    Write-Host "=" * 80
    Write-Host $Title -ForegroundColor Cyan
    Write-Host "=" * 80
    Write-Host ""
}

function Test-Catch2 {
    # Check if Catch2 is available
    try {
        Add-Type -AssemblyName System.Runtime.CompilerServices
        [System.Reflection.Assembly]::LoadWithPartialName("Catch2") 2>$null
        
        if (-not $?) {
            Write-Host "Warning: Catch2 not found as assembly. Searching for pkg-config or vcpkg..." -ForegroundColor Yellow
            
            # Try to find Catch2 in common locations
            $catchPaths = @(
                "$env:VCPKG_ROOT\packages\catch2*\libs",
                "$(Get-Command cmake.exe).Source\lib\cmake\Catch2"
            )
            
            foreach ($path in $catchPaths) {
                if (Test-Path $path) {
                    Write-Host "Using Catch2 from: $path" -ForegroundColor Green
                    return $true
                }
            }
            
            # Try to install via vcpkg
            Write-Host "Trying to install Catch2 via vcpkg..." -ForegroundColor Yellow
            & vcpkg.exe install catch2 --triplet=x64-windows 2>$null
            if ($LASTEXITCODE -eq 0) {
                return $true
            }
        }
    } catch {
        Write-Host "Catch2 check failed: $_" -ForegroundColor Red
    }
    
    # Try cmake to find Catch2
    & cmake -E echo "Searching for Catch2..."
    $result = & cmake --find-package Catch2 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Found Catch2" -ForegroundColor Green
        return $true
    }
    
    Write-Host "ERROR: Catch2 not found. Install it using:" -ForegroundColor Red
    Write-Host "  vcpkg install catch2" -ForegroundColor Red
    Write-Host "  # or manually download from https://github.com/catchorg/Catch2" -ForegroundColor Red
    return $false
}

function Test-SDL2 {
    if (-not (Get-Command cmake.exe -ErrorAction SilentlyContinue)) {
        Write-Host "ERROR: CMake is not installed. Install from https://cmake.org/" -ForegroundColor Red
        return $false
    }
    
    # Try to find SDL2
    sdlFound = & cmake -E echo "Checking for SDL2..." 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: SDL2 not found. Install it using:" -ForegroundColor Red
        Write-Host "  vcpkg install sdl2" -ForegroundColor Red
        return $false
    }
    
    return $true
}

function Test-Dependencies {
    Write-Header "Checking Dependencies"
    
    if (-not (Test-Catch2)) {
        exit 1
    }
    
    if (-not (Test-SDL2)) {
        exit 1
    }
    
    Write-Host "All dependencies found!" -ForegroundColor Green
}

function Build-Tests {
    param(
        [string]$BuildDir = "build_tests",
        [string]$Config = "Release"
    )
    
    Write-Header "Building Tests"
    
    $cmakeArgs = @(
        "-S", ".",
        "-B", $BuildDir,
        "-G", "MinGW Makefiles",
        "-DCMAKE_BUILD_TYPE=$Config"
    )
    
    # Check for SDL2_DIR path
    if ($env:SDL2_DIR) {
        $cmakeArgs += "-DSDL2_DIR=$env:SDL2_DIR"
    } else {
        # Try to find it in vcpkg or toolchain
        $sdlPath = "work/toolchain/sdl2/SDL2-2.32.10/x86_64-w64-mingw32/lib/cmake/SDL2"
        if (Test-Path $sdlPath) {
            $cmakeArgs += "-DSDL2_DIR=$sdlPath"
        } else {
            # Check vcpkg
            $vcpkgSdl = "$env:VCPKG_ROOT\installed\x64-windows\lib\cmake\SDL2"
            if (Test-Path $vcpkgSdl) {
                $cmakeArgs += "-DSDL2_DIR=$vcpkgSdl"
            } else {
                Write-Host "ERROR: SDL2_DIR environment variable not set and default paths not found." -ForegroundColor Red
                exit 1
            }
        }
    }
    
    # Check for Catch2
    if ($env:CATCH2_ROOT) {
        $cmakeArgs += "-DCATCH2_ROOT=$env:CATCH2_ROOT"
    } else {
        $cmakePath = "$(Get-Command cmake.exe).Source\lib\cmake\Catch2"
        if (Test-Path $cmakePath) {
            $cmakeArgs += "-DCATCH2_ROOT=$cmakePath"
        } else {
            Write-Host "WARNING: Catch2 not found. Tests will be built without test framework." -ForegroundColor Yellow
        }
    }
    
    & cmake @cmakeArgs
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed!" -ForegroundColor Red
        exit 1
    }
    
    # Build
    $buildCmd = "& cmake --build $BuildDir --config $Config"
    Write-Host "Running: $buildCmd" -ForegroundColor Gray
    
    & $buildCmd
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Build successful!" -ForegroundColor Green
}

function Run-Tests {
    param(
        [string]$Executable,
        [string]$Args = ""
    )
    
    Write-Header "Running Tests"
    
    if (-not (Test-Path $Executable)) {
        Write-Host "ERROR: Test executable not found at: $Executable" -ForegroundColor Red
        exit 1
    }
    
    $fullArgs = $Args + " --success first"
    & $Executable $fullArgs
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "All tests passed!" -ForegroundColor Green
    } else {
        Write-Host ""
        Write-Host "Some tests failed. See output above for details." -ForegroundColor Red
    }
}

function Test-CurrentBuild {
    # Use the existing build directory from the main project
    $mainBuild = "build"
    $testExe = Join-Path $mainBuild "buraco_test.exe"
    
    Write-Header "Testing Current Build"
    
    if (-not (Test-Path $testExe)) {
        Write-Host "Test executable not found at: $testExe" -ForegroundColor Yellow
        Write-Host "Run 'build.ps1' first to build the project." -ForegroundColor Yellow
        exit 1
    }
    
    Run-Tests -Executable $testExe
    
    # Also test with determinism flag for reproducibility checks
    $testExeDet = Join-Path $mainBuild "buraco_test-deterministic.exe"
    if (Test-Path $testExeDet) {
        Write-Host ""
        Write-Host "Testing with deterministic mode:" -ForegroundColor Cyan
        
        Run-Tests -Executable $testExeDet
    }
}

function Test-NewBuild {
    # Build and test from scratch
    Build-Tests
    
    # Find the test executable (might be in different locations)
    $testExe = Get-ChildItem -Path "." -Filter "buraco_test.exe" -Recurse | Select-Object -First 1
    
    if ($testExe) {
        Run-Tests -Executable $testExe.Path
    }
}

# Main script logic
$scriptName = Split-Path -Leaf $MyInvocation.MyCommand.Path

if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "ERROR: Must run from the tests directory" -ForegroundColor Red
    exit 1
}

Write-Header "Buraco Port Unit Tests Runner"

$command = if ($args.Count -gt 0) { $args[0] } else { "current-build" }

switch ($command) {
    "dependencies" { Test-Dependencies }
    "build" { Build-Tests }
    "run" { 
        if (Test-Path "buraco_test.exe") {
            Run-Tests -Executable ".\buraco_test.exe" -Args "--success first"
        } else {
            Write-Host "ERROR: Test executable not found. Use 'build' command first." -ForegroundColor Red
        }
    }
    "current-build" { Test-CurrentBuild }
    "new-build" { Test-NewBuild }
    default {
        Write-Host "Usage:" -ForegroundColor Cyan
        Write-Host "  .\run_tests.ps1 dependencies     # Check prerequisites" -ForegroundColor White
        Write-Host "  .\run_tests.ps1 build            # Build tests" -ForegroundColor White
        Write-Host "  .\run_tests.ps1 run              # Run existing test executable" -ForegroundColor White
        Write-Host "  .\run_tests.ps1 current-build    # Test the current project build" -ForegroundColor White
        Write-Host "  .\run_tests.ps1 new-build        # Build and test from scratch" -ForegroundColor White
    }
}
