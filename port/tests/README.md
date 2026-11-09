# Testes do porte

Testes unitários do porte C++/SDL2. O harness é próprio (`test_framework.*`)
e não depende de Catch2 nem de nenhum outro pacote: só da biblioteca padrão e
do SDL2 já usado pelo jogo.

## Como compilar e rodar

```powershell
cd port
.\build.ps1                              # compila buraco.exe + buraco_tests.exe
ctest --test-dir build --output-on-failure
# ou direto:
.\build\tests\buraco_tests.exe
# um caso específico (substring do nome):
.\build\tests\buraco_tests.exe "canastra"
```

Os testes são compilados por padrão. Para desligá-los:

```powershell
cmake -S port -B port\build -DBURACO_BUILD_TESTS=OFF ...
```

O script `run_tests.ps1` faz o build e a execução em um passo.

## Arquivos

| Arquivo | Conteúdo |
|---|---|
| `test_framework.hpp/.cpp` | `TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE` e o `main` |
| `test_helpers.h` | criação de saves em memória, `snapshot()` do estado |
| `cards_test.cpp` | ids/naipes, pontos, curingas, glifos, construção de jogos |
| `game_test.cpp` | distribuição, compra, jogos, morto, batida, pontuação, IA, save/load |
| `save_test.cpp` | `sanitizeName`, nomes reservados do Windows, numeração de saves |
| `config_test.cpp` | arquivo `buraco.cfg`, defaults, clamps, valores inválidos e round-trip |
| `window_scale_test.cpp` | tamanho da janela por escala, encaixe na área útil e escala inteira |
| `window_state_test.cpp` | posição fora dos monitores, resize/move do usuário, maximizada |
| `options_test.cpp` | navegação da tela F9, escala, toggles, reset e desenho |
| `report_test.cpp` | `stateLine`/`strictLine` e `checkInvariants` (108 cartas) |
| `ui_test.cpp` | geometria da tela, telas de ajuda/nome/contagem e hash dourado |

## O que é coberto

* **Cartas e jogos**: todos os ids 1..108, tabela de pontos, curingas (2 e
  joker), A baixo/alto, buracos preenchidos por curinga, canastra limpa/suja/
  real e extensão de jogos.
* **Máquina de turnos**: nova partida determinística por seed, 108 cartas
  distribuídas exatamente uma vez, compra do monte/mesa, `layOptions`,
  `layMeld` (novo/aumentar/alvo explícito/limite de 15), descarte, morto,
  batida, encerramento de mão e partida até 3.000 pontos.
* **Persistência**: ida-e-volta completa (`snapshot`), rejeição de arquivos
  corrompidos sem alterar a partida viva, saves antigos sem `stats`, validação
  de faixas e contagens.
* **Saves em disco**: nomes inválidos/reservados (`CON`, `COM1`, ...), corte em
  60 caracteres e numeração `nome2.sav`, `nome3.sav` sem sobrescrever.
* **Invariantes**: duplicação, id inválido, buraco em jogo, jogo vazio e
  contagem total diferente de 108.
* **Tela**: layout 80×25, molduras, mão/mesa/jogos, telas de ajuda, nome e
  contagem. O caso *hash dourado* reproduz `--auto 400 --seed 1234` e compara
  o hash FNV-1a das 25 linhas com o valor capturado antes da divisão de
  `main.cpp` em módulos.
* **Janela**: posição salva fora dos monitores volta a centralizar, resize do
  usuário vira tamanho explícito (`scale=0`), eventos do próprio jogo
  (tamanho/posição aplicados) são ignorados e maximizada/restaurada é
  acompanhada. A lógica pura fica em `window_state.*`; o SDL só fornece os
  eventos e retângulos dos monitores.
* **Opções (F9)**: navegação circular, escala Auto/1×..8×, toggles de ajuste
  inteiro, tela cheia, VSync e maximizada, centralizar, restaurar padrões e
  fechar; o desenho mostra a linha selecionada e os valores atuais.

## Regressão de comportamento

Além dos testes, o porte mantém modos headless determinísticos. Para comparar
uma mudança com uma versão anterior:

```powershell
.\build\buraco.exe --auto 400 --seed 1234 --out auto400.txt
.\build\buraco.exe --check 2000 --seed 4242 --out check2000.txt
.\build\buraco.exe --savetest 400 --seed 99 --save st.sav --out savetest.txt
```

`auto400.txt` e `check2000.txt` devem sair byte a byte iguais; em
`savetest.txt` só o caminho do arquivo muda.

## Adicionando um teste

1. Crie `tests/alguma_test.cpp` (ou use um arquivo existente).
2. Inclua `test_framework.hpp` (e `test_helpers.h` se precisar de saves).
3. Escreva `TEST_CASE("descrição") { ... }` com `CHECK`/`CHECK_EQ`.
4. Adicione o `.cpp` em `tests/CMakeLists.txt`.
