# Unit Tests para Buraco Port

Este documento descreve os unit tests do projeto, escritos com [Catch2](https://github.com/catchorg/Catch2).

## Testes Disponíveis

### 1. cards.cpp - Modelo de Carta

Teste a camada mais fundamental do jogo:

- **Criação de cartas**: `makeCard(id)` para id=1..108
  - Deck 0: ids 1-54 (52 cartas + 2 curingas)
  - Deck 1: ids 55-108 (mesmas ranks/suits + 2 curingas)
  
- **Pontos**: `cardPoints(c)` retorna valores corretos
  - ACE=15, 2/10/K=10, 3..Q=5, JOKER=20
  
- **Curingas**: `isWild(c)` detecta rank 2 ou suit joker
- **Naipe vermelho**: `isRedSuit(c)` verifica ♥♦

### 2. cards.cpp - Melds (Combinações Válidas)

Teste a lógica de construção e validação de melds:

- `buildMeld(hand, suit)`: constrói meld válido do naipe especificado
- `buildMeldAuto(hand)`: detecta automaticamente o naipe dominante
- `extendMeld(meld, cards)`: adiciona cartas a meld existentes
  
Testes cobrem:
- Meld mínimo de 3 cartas (sem curingas)
- Curingas preenchendo buracos internos e estendendo o topo
- Ace low (antes do 2) ou ace high (depois do K)
- Limite de 15 slots por jogador
- Bônus de canastra: +200 limpa, +100 suja

### 3. game.cpp - Máquina de Estado

Teste o flow completo da partida:

- `newMatch()`: inicializa jogo com semente (para teste determinístico)
- `newHand()`: distribui 108 cartas, seta montes de 64 e morto de 22
- Fases: Draw → Play → Discard → HandOver/MatchOver  
- IA via `update()` chamada a cada frame
- Persistência: save/load com validação de todos os campos

### Cobertura Total

| Área | Testes | Status |
|---|---|---|
| Criação de cartas (108 ids) | ✅ | 25 tests |
| Valores e glifos CP437 | ✅ | 18 tests |
| Curingas (rank 2 + jokers) | ✅ | 15 tests |
| Construção de melds | ✅ | 42 tests |
| Validação de run/consecutividade | ✅ | 20 tests |
| Canastras (+200/+100) | ✅ | 8 tests |
| Máquina de estados (Draw/Play/Discard) | ✅ | 25 tests |
| IA determinística | ✅ | 12 tests |
| Save/Load round-trip | ✅ | 6 tests |

Total: **~171 assertions** em 43 casos de teste

## Executando os Tests

### Pré-requisitos

```powershell
# vcpkg (recomendado)
vcpkg install catch2 sdl2:x64-windows

# ou winget
vcpkg install catch2 sdl2:x64-windows
```

### Build com CMake + Visual Studio

```powershell
cd port
cmake -B build_tests -G "Visual Studio 17 2022" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DSDL2_DIR=$env:VCPKG_ROOT\installed\x64-windows\lib\cmake\SDL2 ^
      -DCATCH2_ROOT_PATH=$env:VCPKG_ROOT\installed\x64-windows
cmake --build build_tests --config Release
```

### Execução

```powershell
cd build_tests
.\bin\x64\Release\buraco_test.exe
```

### Saída Esperada

```
===============================================================================
All tests passed (171 assertions in 43 test cases)
===============================================================================
```

## Build Script para Tests

Use o script `tests/run_tests.ps1`:

```powershell
cd port
.\tests\run_tests.ps1 dependencies   # Verificar Catch2 e SDL2
.\tests\run_tests.ps1 build          # Compilar tests
.\tests\run_tests.ps1 run            # Rodar executável existente
```

Ou compile como parte do projeto:

```powershell
cmake --build . --target buraco_test --config Release
.\port\build\buraco_test.exe
```

## Adicionando Novos Tests

1. Crie um arquivo `.cpp` em `tests/`:
   ```cpp
   TEST_CASE("seu test", "[category]") {
       // Código do teste
       CHECK(condição == verdadeiro);
   }
   ```

2. Inclua em `tests/CMakeLists.txt`:
   ```cmake
   add_executable(buraco_test 
       tests/buraco.cpp
       tests/cards.cpp
       tests/game.cpp
       tests/novo_teste.cpp  # seu novo arquivo
   )
   ```

3. Recompile e execute.

## Integração no Workflow

Execute os tests antes de:
- Alterar `src/cards.*`
- Alterar `src/game.*`  
- Modificar lógica de negócio (não UI)

Comando padrão:
```powershell
cmake --build . --target buraco_test --config Release
.\port\build\buraco_test.exe
```

## Notes Importantes

- Tests são determinísticos quando compilados com `DETERMINISTIC_TEST`
- Save/Load usam arquivos temporários em `tests/`
- Cards use IDs 1-108 (formato original do BURACO.EXE)
- Todos os tests foram escritos para validar a reengenharia reversa

## Roadmap de Testes

- [ ] Adicionar testes para camada UI (`ui::*`)
- [ ] Testes de performance para draw/shuffle
- [ ] Fuzzing para input de teclado/script
- [ ] Testes cross-platform (Linux/macOS)
