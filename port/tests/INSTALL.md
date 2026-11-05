# Instalação e Execução dos Unit Tests

Este documento descreve como configurar, compilar e executar os unit tests do projeto Buraco.

## Visão Geral

Os unit tests usam o framework **Catch2** para validar:

1. **Modelo de Carta** (`cards.cpp`): 37 testes
   - Criação de cartas (IDs 1-108)
   - Valores, glifos CP437, curingas
   
2. **Construção de Melds** (`cards.cpp`): 45 testes
   - `buildMeld()`, `buildMeldAuto()`, `extendMeld()`
   - Validação de run consecutiva
   - Cálculo de pontos e bônus

3. **Máquina de Estado** (`game.cpp`): 35 testes
   - Fases (Draw/Play/Discard)
   - IA determinística
   - Save/Load com validação

Total: **~117 assertions em 40 casos de teste**

## Pré-requisitos

### Windows (Recomendado)

```powershell
# Instalar vcpkg se ainda não instalado
git clone https://github.com/Microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

# Instalar dependências
vcpkg install sdl2:x64-windows
vcpkg install catch2:x64-windows
```

### Linux / macOS

```bash
sudo apt-get install libsdl2-dev  # Ubuntu/Debian
brew install sdl2                  # macOS (Homebrew)

# Clone Catch2 (opcional - ou use a versão instalada pelo sistema)
git clone https://github.com/catchorg/Catch2.git --depth 1
cd Catch2
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make install
```

## Build com Visual Studio + vcpkg

### Passo 1: Configurar CMake

```powershell
cd port
vcpkg install sdl2 catch2:x64-windows

cmake -B build_tests -G "Visual Studio 17 2022" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DSDL2_DIR=$env:VCPKG_ROOT\installed\x64-windows\lib\cmake\SDL2 ^
      -DCATCH2_ROOT_PATH=$env:VCPKG_ROOT\installed\x64-windows
```

### Passo 2: Compilar

```powershell
cmake --build build_tests --config Release
```

### Passo 3: Executar Tests

```powershell
cd build_tests
.\bin\x64\Release\buraco_test.exe
```

## Build com MinGW-w64 (Windows)

### Passo 1: Instalar dependências

```powershell
# winget é recomendado
winget install Kitware.CMake
winget install SDL2  # ou baixe SDL2-devel-*-mingw.zip manualmente

# Ou use o toolchain existente no projeto
$env:SDL2_DIR = "work\toolchain\sdl2"
```

### Passo 2: Configurar CMake

```powershell
cd port

cmake -B build_tests -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DSDL2_DIR="$env:SDL2_DIR\lib\cmake\SDL2" ^
      -DCMAKE_CXX_STANDARD=20
```

### Passo 3: Compilar

```powershell
cmake --build build_tests --config Release
```

### Passo 4: Executar

```powershell
cd build_tests
Release\buraco_test.exe
```

## Build Script Automático (PowerShell)

O repositório inclui o script `tests/run_tests.ps1`:

```powershell
cd port

# Verificar dependências
.\tests\run_tests.ps1 dependencies

# Construir com toolchain existente
cmake -S . -B build_tests -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DSDL2_DIR="work\toolchain\sdl2\SDL2-2.32.10\x86_64-w64-mingw32\lib\cmake\SDL2"

# Ou com vcpkg
vcpkg install sdl2:x64-windows catch2:x64-windows
cmake -B build_tests_vs -G "Visual Studio 17 2022" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DSDL2_DIR=$env:VCPKG_ROOT\installed\x64-windows\lib\cmake\SDL2 ^
      -DCATCH2_ROOT_PATH=$env:VCPKG_ROOT\installed\x64-windows
```

## Executando o Build do Projeto Principal + Tests

Para incluir os tests no build normal:

### Passo 1: Build normal

```powershell
.\build.ps1  # ou execute cmake manualmente
```

### Passo 2: Add target de teste

```powershell
# Adicione a CMakeLists.txt:
add_subdirectory(tests)

# Ou compile explicitamente:
cmake --build . --target buraco_test --config Release
```

### Passo 3: Executar tests

```powershell
.\port\build\buraco_test.exe
```

## Output Esperado

### Sucesso

```
===============================================================================
All tests passed (XX assertions in XX test cases)
===============================================================================
```

### Falha (exemplo)

```
test/cards.cpp:256: FAILED:
  bool(ok) == true
with expansion:
  false == true
```

## Debug Mode

Para depurar:

```powershell
cmake -B build_tests_debug -G "Visual Studio 17 2022" ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DSDL2_DIR=$env:VCPKG_ROOT\installed\x64-windows\lib\cmake\SDL2 ^
      -DCATCH2_ROOT_PATH=$env:VCPKG_ROOT\installed\x64-windows

# Run com Visual Studio
.\build_tests_debug\bin\x64\Debug\buraco_test.exe
```

## Troubleshooting

### "Catch2 not found"

```powershell
vcpkg install catch2:x64-windows
```

### "SDL2 not found"

```powershell
vcpkg install sdl2:x64-windows
# ou copie SDL2.dll para a pasta do executável
Copy-Item "$env:VCPKG_ROOT\installed\x64-windows\bin\SDL2.dll" .
```

### Build lento com Visual Studio

Adicione em `CMakeLists.txt`:

```cmake
set(CMAKE_MSVC_DEBUG_INFORMATION_PATH "$ENV\\VsDebugInformation")
```

## Testes Determinísticos

Para garantir resultados reproduzíveis em CI:

```powershell
# Compile com flag DETERMINISTIC_TEST
cmake -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_CXX_FLAGS="/DDETERMINISTIC_TEST"  # MinGW
```

## Rodar Tests como Parte de CI

Exemplo para GitHub Actions ou Azure DevOps:

```yaml
- name: Install dependencies
  run: |
    vcpkg install sdl2 catch2:x64-windows

- name: Build tests
  shell: PowerShell
  workingDirectory: port
  script: |
    cmake -B build_tests -G "Visual Studio 17 2022" `
      -DCMAKE_BUILD_TYPE=Release `
      -DSDL2_DIR=$env:VCPKG_ROOT\installed\x64-windows\lib\cmake\SDL2 ^
      -DCATCH2_ROOT_PATH=$env:VCPKG_ROOT\installed\x64-windows
    cmake --build build_tests --config Release

- name: Run tests
  shell: PowerShell
  workingDirectory: port/build_tests
  script: |
    & .\bin\x64\Release\buraco_test.exe
```

## Verificação Completa (Test Suite)

Para rodar todo o suite de testes:

```powershell
cd port

# Build tudo (projet + tests)
.\build.ps1  # ou cmake --build build
cmake -S . -B build_tests_vs -G "Visual Studio 17 2022" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DSDL2_DIR=$env:VCPKG_ROOT\installed\x64-windows\lib\cmake\SDL2 ^
      -DCATCH2_ROOT_PATH=$env:VCPKG_ROOT\installed\x64-windows
cmake --build build_tests_vs --config Release

# Executar main executável
cd build
.\buraco.exe --auto 100 --seed 42

# Rodar tests
cd ..\build_tests_vs
bin\x64\Release\buraco_test.exe

# Validar save/restore
.\buraco.exe --savetest 500 --seed 12345

# Run checks
.\buraco.exe --check 5000 --seed 777
```

## Recursos Adicionais

- `tests/run_tests.ps1` - Script PowerShell para build e execução
- `tests/README.md` - Documentação detalhada dos testes
- `port/README_tests.md` - Guia completo de unit tests

## Contato e Suporte

Para reportar bugs nos tests ou solicitar novos:
- Issues no repositório principal
- Email do maintainer

---

**Nota:** Os tests são mantidos na branch `main`. Execute antes de fazer push de alterações em `src/cards.*` ou `src/game.*`.
