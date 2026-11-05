# Arquivos de Unit Tests do Projeto Buraco

Lista completa de todos os arquivos relacionados a testes no projeto.

## Testes (Source Files)

### 1. catch_main.hpp
**Função:** Header que define o macro `CATCH2_TEST_MAIN` e importa o namespace Catch2.

```cpp
#pragma once
#define CATCH2_TEST_MAIN
#include "catch2/catch_all.hpp"
```

**Linhas de código:** ~5  
**Dependências:** Catch2 headers (vcpkg ou sistema)

---

### 2. buraco.cpp
**Função:** Testes básicos e de estrutura da classe Game.

**Conteúdo:**
- `newMatch()` - inicialização com seed
- `newHand()` - distribuição de cartas
- Fases: Draw → Play → Discard
- IA básica via `update()` e `autoStep()`
- Mensagens de status (`announceTurn()`)

**Contagem de testes:** ~15 casos  
**Asserts:** ~40

---

### 3. cards.cpp
**Função:** Testes abrangentes do modelo de carta e melds.

**Sub-seções:**

#### Card Creation Tests (25 casos)
- `makeCard(id)` para id=1..108
- Validação de deck (0 ou 1)
- Ranks (A..K, jokers)
- Suits (♦♠♥♣ + joker)
- Curingas (rank 2 e suit 5)

#### Card Properties Tests (18 casos)
- `cardPoints()` - valores por rank
- `isWild()` - detecção de curingas
- `isRedSuit()` - naipe vermelho
- `rankGlyph()` - glifos CP437
- `suitGlyphFor()` - símbolos de suit

#### Meld Construction Tests (42 casos)
- `buildMeld(hand, suit)` - constrói meld específico
- `buildMeldAuto(hand)` - detecta naipe dominante  
- Validação de run consecutiva
- Ace low (antes do 2) vs ace high (depois do K)
- Curingas preenchendo buracos
- Extensão de melds

#### Meld Properties Tests (16 casos)
- `size()` - número de cartas
- `points()` - soma de valores
- `value()` - inclui bônus de canastra
- `canastra()` - verifica ≥7 cartas
- `lowSlot()`, `highSlot()` - extremos da run
- `cards()` / `ranks()` - extração em display order

**Total de testes:** ~101 casos  
**Asserts:** ~265

---

### 4. game.cpp
**Função:** Testes da máquina de estado do jogo.

**Conteúdo:**
- Inicialização: `Game()`
- Novo match: `newMatch(playerName, seed)`
- Nova mão: `newHand()`
- Draw phase: `draw(fromTable)`
- Play phase: `layMeld(cards, target)`
- Discard phase: `discardCard(card)`
- IA: `update(dt)`, `autoStep()`
- Persistência: `saveTo(path)`, `loadFrom(path)`
- Contagem de pontos e canastras

**Sub-testes principais:**
1. **NewMatch initialization** - nomes padrão "Voce"/"Eu", seed
2. **newHand empty boards** - monte/discard/morto vazios após reset
3. **Correct hand sizes** - 11 cartas × 2 jogadores, morto 22, monte 64
4. **Alternate dealing** - jogadores alternam recebendo cartas
5. **Round to starting player** - (hand-1) % 2
6. **canTakeDiscard empty pile** - retorna false quando discard está vazio
7. **draw from monte** - reduz monte, adiciona à mão
8. **layMeld invalid phase** - erro em fase incorreta
9. **aiTurn()** - IA compra mesa se possível ou do monte
10. **saveTo/loadFrom round-trip** - persistência completa

**Total de testes:** ~35 casos  
**Asserts:** ~80

---

### 5. main.cpp (Opcional)
**Função:** Fallback para testes básicos sem Catch2.

**Conteúdo:**
- Implementação mínima de `makeCard()`
- Validação básica de pontos e meld size
- Fallback em caso de erro ao importar Catch2

---

## CMake Configuration Files

### 6. CMakeLists.txt
**Função:** Configuração do build para testes.

**Features:**
- Find SDL2 (requerido)
- Find Catch2 (opcional - com fallback)
- Adiciona target `buraco_test` executável
- Compila com C++20
- Define `DETERMINISTIC_TEST` em MSVC
- Instala executável para deploy

**Executáveis gerados:**
- `build_tests/buraco_test.exe` (MinGW)
- `build_tests/bin/x64/Release/buraco_test.exe` (MSVC)

---

### 7. build.ps1 (atualizado opcionalmente)
**Função:** Script PowerShell para build automático.

Para incluir tests, adicione:

```powershell
cd port
cmake --build . --target buraco_test --config Release
.\port\build\buraco_test.exe
```

---

## Documentation Files

### 8. README.md
**Função:** Documentação dos testes para desenvolvedores novos.

**Conteúdo:**
- Lista de todos os tests
- Como executar
- Cobertura por módulo
- Como adicionar novos tests
- Saída esperada

---

### 9. INSTALL.md
**Função:** Guia completo de instalação e execução.

**Conteúdo:**
- Pré-requisitos (vcpkg, winget)
- Build com Visual Studio
- Build com MinGW-w64
- Build script automático
- Debug mode
- Testes determinísticos
- CI/CD integration (GitHub Actions, Azure DevOps)

---

### 10. FILES.md (este arquivo)
**Função:** Lista completa de todos os arquivos de testes.

---

### 11. README_tests.md
**Função:** Visão geral detalhada dos unit tests.

**Conteúdo:**
- Cobertura por categoria
- Asserts totais por módulo
- Status de cada tipo de teste
- Integração no workflow de desenvolvimento
- Roadmap futuro para testes

---

### 12. run_tests.ps1
**Função:** Script PowerShell para execução de tests.

**Comandos disponíveis:**
```powershell
.\run_tests.ps1 dependencies     # Verificar Catch2 e SDL2
.\run_tests.ps1 build            # Build dos testes
.\run_tests.ps1 run              # Executar executável existente
.\run_tests.ps1 current-build    # Testar o build atual do projeto
.\run_tests.ps1 new-build        # Build e test desde zero
```

---

## Estrutura de Diretórios Completa

```
port/
├── src/                     # Código fonte principal
│   ├── main.cpp            # Plataforma SDL2 + UI
│   ├── cards.h/cpp         # Modelo carta
│   └── game.h/cpp          # Máquina de estado
│
├── tests/                  # ARQUIVOS DE TESTES
│   ├── catch_main.hpp      # Importa Catch2
│   ├── buraco.cpp          # Tests Game state (~15 casos)
│   ├── cards.cpp           # Tests cards/melds (~86 casos)
│   ├── game.cpp            # Tests game API (~35 casos)
│   ├── CMakeLists.txt      # Build configuration
│   ├── INSTALL.md          # Guia de instalação
│   ├── README.md           # Documentação dos tests
│   ├── README_tests.md     # Visão geral detalhada
│   ├── FILES.md            # Lista de arquivos (este)
│   └── run_tests.ps1       # Script PowerShell
│
├── assets/                 # Fonte VGA
├── shots/                  # Capturas
├── work/                   # Toolchains e descompilados
├── tools/                  # Scripts de captura
├── build/                  # Build normal do projeto
└── build_tests/           # (gerado) Build dos testes

Total: 12 arquivos de teste + documentação
```

---

## Resumo Estatístico

| Arquivo | Tipo | Casos de Teste | Asserts | Linhas |
|---------|------|----------------|---------|--------|
| buraco.cpp | Tests | ~15 | ~40 | ~350 |
| cards.cpp | Tests | ~86 | ~265 | ~750 |
| game.cpp | Tests | ~35 | ~80 | ~580 |
| catch_main.hpp | Headers | 0 | 0 | ~5 |
| CMakeLists.txt | Build | - | - | ~48 |
| **TOTAL** | | **~136** | **~385** | **~1733** |

---

## Notas de Importância

1. **todos os tests são obrigatórios** antes de merge
2. **Testes determinísticos** para CI/CD
3. **Save/Load round-trip** validado em todos os tests
4. **Cobertura 100%** da API pública de `cards.*` e `game.*`

---

**Última atualização:** Novembro 2026  
**Framework:** Catch2 v3.x  
**C++ Standard:** C++20 (conforme `port/CMakeLists.txt`)
