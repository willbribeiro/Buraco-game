# Buraco — porte nativo do `BURACO.EXE` (MS-DOS) para Windows/SDL2

Reimplementação em **C++20 + SDL2** do jogo `BURACO.EXE`, feita a partir da
engenharia reversa do binário original (DOSBox-X + Ghidra + UNP 4.11).

O porte **não** é um emulador: as regras, o baralho, a máquina de turnos e o
desenho da mesa foram reescritos em código nativo, mas mantendo a mesma
apresentação em modo texto 80×25 (glifos CP437 desenhados com a fonte VGA
extraída do próprio jogo).

---

## 1. Estado atual

| Item | Situação |
|---|---|
| Baralho 108 cartas (2 baralhos + 4 curingas) | ✅ |
| Distribuição 11/11, morto de 22, monte de 64 | ✅ |
| Comprar do monte / comprar a mesa (leva **todas** as cartas) | ✅ |
| Baixar jogo, aumentar jogo, vários jogos do mesmo naipe (até 15 slots) | ✅ |
| Escolher entre aumentar um jogo existente ou baixar um novo do mesmo naipe | ✅ |
| Curingas (2 e joker) preenchendo buracos e topo | ✅ |
| Pegar o morto (11 + 11), penalidade de −100 | ✅ |
| Bater (+100 fixo), exigir canastra | ✅ |
| Contagem de pontos, canastra limpa +200 / suja +100 / real +500 | ✅ |
| Tela de contagem jogo por jogo, com soma, bônus e descontos | ✅ |
| Partida até 3.000 pontos, várias mãos | ✅ |
| Sorteio diferente a cada nova partida | ✅ |
| Fonte VGA 8×16 autêntica extraída do DOSBox-X | ✅ |
| F1 ajuda, F3 situação geral em tela cheia; mão do adversário sempre oculta | ✅ |
| F2 salvar por jogador / F5 restaurar (`<nome>.sav` na pasta do jogo) | ✅ |
| Tela inicial, continuar jogo gravado ou novo jogo com nome do jogador | ✅ |
| Efeitos sonoros sintetizados (embaralhar, distribuir, comprar, baixar jogo, canastra, morto, bater, erro), com volume | ✅ |
| Modo headless para regressão/diff (inclusive do fluxo inicial) | ✅ |

## 2. Build

### Toolchain usada na validação

* MinGW-w64 GCC 16.2.0 (UCRT, POSIX, SEH) — `winget install BrechtSanders.WinLibs.POSIX.UCRT`
* CMake 4.4.4 — `winget install Kitware.CMake`
* SDL2 2.32.10 (pacote `SDL2-devel-2.32.10-mingw.zip`)

### Passos

Da raiz do repositório:

```powershell
# atalho: localiza o SDL2 em work\toolchain, configura, compila e copia a DLL
.\port\build.ps1

# ou manualmente:
# SDL2 (uma vez): baixe SDL2-devel-*-mingw.zip e extraia em work\toolchain\sdl2
$sdl = "work\toolchain\sdl2\SDL2-2.32.10\x86_64-w64-mingw32"

cmake -S port -B port\build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
      -DSDL2_DIR="$sdl\lib\cmake\SDL2"
cmake --build port\build

Copy-Item "$sdl\bin\SDL2.dll" port\build\   # o POST_BUILD copia assets/, não a DLL
.\port\build\buraco.exe
```

Com vcpkg/MSVC o fluxo é o mesmo, trocando o gerador:

```powershell
vcpkg install sdl2:x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

O `CMakeLists.txt` copia `assets/` para o lado do executável (a fonte 8×16 é
carregada em `SDL_GetBasePath()/assets/font8x16.bin`).

### Testes

Os testes são compilados por padrão (`-DBURACO_BUILD_TESTS=OFF` desliga) e não
dependem de Catch2:

```powershell
cd port
.\build.ps1
ctest --test-dir build --output-on-failure
# ou: .\build\tests\buraco_tests.exe
```

### Configuração de vídeo e som (fase 1)

O porte procura um `buraco.cfg` ao lado do `buraco.exe` (pasta portátil). Se
esse arquivo não existir ou não puder ser gravado, a configuração fica em
`SDL_GetPrefPath("buraco", "buraco")` (normalmente
`%APPDATA%\buraco\buraco\buraco.cfg`). O arquivo é texto simples:

```ini
# Buraco settings
version=1
fullscreen=0
scale=2
window_width=1280
window_height=800
window_centered=1
window_x=0
window_y=0
maximized=0
integer_scale=0
vsync=1
sound=1
sound_volume=70
```

| Chave | Efeito |
|---|---|
| `fullscreen` | abre em tela cheia borderless |
| `scale` | `0` = tamanho salvo; `1..8` = 640×400 × escala |
| `window_width` / `window_height` | tamanho da janela quando `scale=0` |
| `window_centered` | `1` centraliza; `0` usa `window_x`/`window_y` |
| `window_x` / `window_y` | posição da janela (aceita coordenadas negativas) |
| `maximized` | abre maximizada |
| `integer_scale` | escala inteira (pixel-perfect) |
| `vsync` | VSync ligado/desligado |
| `sound` | efeitos sonoros ligados/desligados |
| `sound_volume` | volume dos efeitos em `0..100` (passos de 10 na tela F9) |

Na inicialização o tamanho é reduzido para caber em 90% da área útil da tela,
preservando 8:5. O padrão é 2× (1280×800); em telas como 1366×768 ele encolhe
automaticamente. O modo `integer_scale` pode cair para 1× nessas telas — é o
comportamento esperado do modo pixel-perfect. Se a posição salva não cair em
nenhum monitor conectado, a janela é centralizada (`--display-info` mostra o
diagnóstico).

Durante o jogo, `F11` ou `Alt+Enter` alternam entre janela e tela cheia
(borderless, na resolução da área de trabalho). Ao fechar, o porte grava
tamanho, posição, maximizada, tela cheia, escala, escala inteira, VSync, som e
volume no `buraco.cfg`. Para não sobrescrever as preferências do jogador, a
gravação é omitida quando a execução usa opções de vídeo ou som na linha de
comando (`--scale`, `--fullscreen`, `--integer-scale`, `--vsync`, `--sound`,
`--volume`) e em execuções de demonstração (`--auto ... --gui`).

Opções de linha de comando relacionadas (válidas só para a execução; não
alteram o arquivo):

```powershell
.\build\buraco.exe --display-info          # mostra config, display, som e escala calculada
.\build\buraco.exe --scale 3 --vsync off
.\build\buraco.exe --fullscreen --integer-scale
.\build\buraco.exe --sound off --volume 40
.\build\buraco.exe --audio-test            # toca todos os efeitos pelo dispositivo real
.\build\buraco.exe --config work\meu.cfg --reset-config
```

A tela de opções (`F9`) edita tudo isso em tempo real: escala (Auto ou 1×..8×),
ajuste inteiro/esticado, tela cheia, VSync, iniciar maximizada, som, volume,
centralizar a janela e restaurar os padrões (vídeo e som). Setas cima/baixo
escolhem a linha, esquerda/direita alteram o valor (volume em passos de 10%),
`ENTER` aplica e `F9`/`ESC` fecha. A partida fica pausada enquanto a tela está
aberta, e as preferências são gravadas no `buraco.cfg` ao sair.

## 3. Controles

| Tecla | Ação |
|---|---|
| ← / → | escolhe a carta sob o cursor (marcador `▼` na linha 17) |
| ↑ / ↓ | rola a lista de jogos na mesa (quando passa de 6) |
| Espaço | marca/desmarca a carta para baixar |
| ENTER (fase compra) | compra do monte |
| S / N | responde "Comprar a mesa (S/N) ?" (S leva a mesa inteira) |
| ENTER (fase jogo) | baixa as cartas marcadas; sem marcação → vai para o descarte |
| A / N (pergunta) | aumenta o jogo existente (A) ou baixa um novo do mesmo naipe (N) |
| ENTER (fase descarte) | descarta a carta sob o cursor |
| ENTER (fim de mão) | próxima mão |
| ENTER (tela inicial) | segue para continuar um save encontrado ou pede o nome |
| S / N (tela de save) | continua o jogo gravado (S) ou começa um novo (N) |
| texto + ENTER (nome) | o nome digitado vira o arquivo de progresso |
| F1 | tela de ajuda (o mesmo texto da tela inicial) |
| F3 | situação geral em tela cheia (partidas, vitórias, pontos, mãos, jogos, mesa e monte) |
| F2 | grava a partida em `<nome do jogador>.sav` (ou `<nome>2.sav`, `<nome>3.sav`, ...) |
| F5 | restaura a partida gravada no caminho atual |
| F9 | opções de vídeo e som: escala, ajuste, tela cheia, VSync, maximizada, som, volume e posição |
| F11 / Alt+Enter | alterna janela / tela cheia (a preferência é salva ao sair) |
| ESC | limpa a seleção; sem seleção, encerra (na tela inicial, encerra) |

### Fluxo inicial

Ao abrir o jogo aparece a tela de apresentação/ajuda (a mesma do `F1`), com o
texto terminando em `...agora aperte ENTER`. Depois do `ENTER`, o porte procura
na pasta do jogo (ao lado de `buraco.exe`) o `.sav` mais recente que comece com
`BURACO-SAVE`:

* **se encontrar**, pergunta se o jogador quer continuar (`S`) ou criar um novo
  jogo (`N`);
* **se não encontrar** (ou se o jogador escolher `N`), pede o nome do jogador.

O nome digitado é usado como nome do arquivo de progresso. Se já existir um
arquivo com esse nome, o porte acrescenta numeração progressiva
(`joao.sav`, `joao2.sav`, `joao3.sav`, ...). `F2` grava no arquivo escolhido no
início da partida; `F5` restaura esse mesmo caminho.

## 4. Modos de execução (teste sem janela)

O mesmo executável tem um modo *headless* determinístico, usado para batizar o
motor de regras sem depender de foco de janela (comandos a partir de `port\`):

```powershell
# auto-jogo: as duas mãos são jogadas pela IA; grava tela + estado a cada passo
.\build\buraco.exe --auto 4000 --out work\auto.txt --seed 777

# replay de teclas: um token por linha (ENTER, LEFT, RIGHT, SPACE, S, N, F1,
# F2, F5, PGUP, ESC), imprime a tela 80x25 e o estado após cada passo
.\build\buraco.exe --script work\keys_turns.txt --out work\screen.txt --seed 1234

# auto-jogo até um ponto e depois abre a janela (para screenshots)
.\build\buraco.exe --auto 180 --gui --seed 777

# invariantes: joga partidas inteiras e confere que as 108 cartas continuam
# contabilizadas; falha com FAIL + estado no arquivo de saída
.\build\buraco.exe --check 20000 --out work\check.txt --seed 777

# ida-e-volta do save: grava, recarrega e compara o estado por mais 400 fases
.\build\buraco.exe --savetest 900 --out work\savetest.txt --save work\test.sav --seed 31337

# fluxo inicial sem janela: apresentação -> nome -> F2 -> F3
.\build\buraco.exe --flow --script work\keys_flow_new.txt --out work\flow.txt --savedir work\flowtest --seed 4242
```

`--save <arquivo>` troca o caminho padrão do save e também a pasta usada na
descoberta do fluxo inicial (útil para testes sem sobrescrever um save real).
`--savedir <pasta>` define explicitamente a "pasta do jogo" para descoberta e
numeração dos saves. `--flow` faz um `--script` headless começar na tela de
apresentação (em vez de direto na mesa), permitindo testar o fluxo
continuar/novo jogo e a digitação do nome. `--check` imprime uma linha por mão fechada
(`# hand N scored at step S: p0 ... p1 ...`) e termina com
`OK steps=... hands=... p0=... p1=...`.

Sem `--seed`, o jogo interativo sorteia uma semente nova por partida (relógio
de alta resolução + `random_device`), então as cartas nunca saem na mesma
ordem; o modo headless mantém a semente 1234 para os testes serem
reproduzíveis. `--seed N` fixa a semente nos dois modos.

Formato do dump: blocos `# step N: TECLA`, 25 linhas de 80 colunas em CP437 e
uma linha `# state` com fase, jogador da vez, RNG, mão, mesa, morto, monte,
jogos (naipe, tamanho, valor e ids das cartas) e placar. Suits são
`1 ♦`, `2 ♠`, `3 ♥`, `4 ♣`, `5` curinga; ranks `1..13`, `15` = joker.

## 5. Arquitetura

```
src/main.cpp        entrada, argumentos, teclado e modos headless
src/cards.*         modelo de carta, validação de jogo, valor de canastra
src/game.*          máquina de turnos, IA, contagem de pontos
src/game_save.cpp   gravação/restauração de partidas
src/ui.*            apresentação 80x25 da mesa e das telas
src/video.*         framebuffer de texto (80x25)
src/renderer.*      fonte VGA + apresentação SDL2
src/save.*          descoberta e nomes dos arquivos de progresso
src/report.*        dumps de estado e invariantes das 108 cartas
tests/              testes unitários (harness próprio, sem Catch2)
assets/             font8x16.bin (256 glifos × 16 bytes, VGA 8×16)
```

O porte escreve direto no framebuffer `dos::Video::text` (80×25×2). O antigo
*shim* de BIOS/DOS (`INT 10h/16h/33h`, DAC VGA, VRAM `B800:0000`/`A000:0000`)
foi removido junto com o modo 13h: o jogo só usa modo texto, e `renderer.cpp`
transforma os glifos CP437 em pixels com a fonte extraída do jogo.

### `ui::` — geometria da tela

Constantes recuperadas de `FUN_1000_1920` (réguas e divisória) e de
`FUN_1000_258a` (mão):

| Constante | Valor | Origem |
|---|---|---|
| linha da mensagem | 0 | `FUN_1000_05ad` |
| réguas horizontais | 1, 7, 15, 24 | `FUN_1000_1920` |
| divisória vertical | coluna 39, linhas 8–14 (`┬` 7, `┴` 15) | `FUN_1000_1920` |
| morto | coluna 1, linha 2 (duas costas sobrepostas em +2) | caixa `DS:0x50` |
| mesa (descarte) | coluna 11, linha 2, leque para a direita | caixa `DS:0x50` |
| rótulo da mão | linha 16 | `FUN_1000_258a` |
| cartas da mão | linha 19, coluna 1 | `DS:0x58/0x59` |
| carta selecionada | uma linha acima (18) | `FUN_1000_258a` |
| marcador do cursor | `▼` (0x1F) na linha 17 | idem |

### `buraco::` — regras

* 108 cartas: `id 1..52` baralho 0, `53/54` jokers, `55..106` baralho 1,
  `107/108` jokers. Naipe `1 ♦`, `2 ♠`, `3 ♥`, `4 ♣`, `5` joker.
* Curinga = rank 2 **ou** naipe 5 (`FUN_1000_1c6c`); vale 10 (2) / 20 (joker).
* Pontos por rank: `A=15`, `2=10`, `3..7=5`, `8..K=10`, `A alto=15`.
* Jogo: um naipe, ranks consecutivos, ≥3 cartas; curingas preenchem buracos
  internos e/ou estendem o topo. Cada jogador tem 15 slots de jogo, sem limite
  por naipe; aumentar um jogo já baixado aceita qualquer número de cartas
  (o novo jogo é que precisa de ≥3).
* Comprar a mesa leva **todas** as cartas do descarte, não só a última; a
  última carta comprada vira `lastDrawn_` e é a primeira que a IA tenta baixar.
* Canastra: ≥7 cartas → +200 limpa, +100 suja (com curinga). Canastra real
  (sequência completa A–K do mesmo naipe, sem curinga, valendo 500 no total em
  vez de 200) → +500. Nela o 2 do naipe conta como carta natural, já que o 2 é
  curinga nos demais jogos.
* Bater: mão vazia com canastra; +100 fixo, tanto baixando todas as cartas
  quanto descartando a última. (O executável original dava 200 ao baixar tudo;
  o porte segue a regra atual do projeto.)
* Morto: 22 cartas; quem esvazia a mão pega 11; quem nunca pegou perde 100.
* Monte vazio: o original sorteia a carta 0 e o jogador não compra nada
  (`FUN_1000_2a03` devolve 0 quando os 108 slots estão vazios) — a mão encolhe
  a cada descarte até alguém bater.
* Partida até 3.000 pontos.

### Save (`F2`/`F5`)

Arquivo texto `BURACO-SAVE 1` gravado na pasta do jogo (ao lado de
`buraco.exe`). O nome padrão vem do jogador: `<nome>.sav`, com numeração
progressiva (`<nome>2.sav`, `<nome>3.sav`, ...) quando o nome já existe;
`--save <arquivo>` e `--savedir <pasta>` substituem esse caminho nos testes.
Guarda fase, jogador da vez, semente do RNG, mãos, jogos (15 slots + flags de
curinga), monte, mesa, morto, placar, a mensagem e as estatísticas da partida
(`stats <partidas> <vitórias 0> <vitórias 1>`). O load valida os ids (1..108) e
os contadores antes de substituir o estado; um arquivo corrompido é recusado
por inteiro, sem quebrar a partida em andamento. Saves antigos sem a linha
`stats` continuam válidos (as estatísticas começam zeradas).

## 6. Fonte VGA

`assets/font8x16.bin` tem 4.096 bytes = 256 glifos × 16 linhas, extraídos de
uma execução de `ALLCHARS.COM` no DOSBox-X com `windowresolution=720x400`
(célula 9×16, glifo de 8 px começando na coluna 0 do grid, topo em `y=51`).
Conferido contra o bitmap canônico do BIOS:

```
'A' = 00 00 10 38 6C C6 C6 FE C6 C6 C6 C6 00 00 00 00
```

Ferramentas: `tools/extract_font.py`, `tools/dosfont.py`, `tools/read_screen.py`.

## 7. Divergências conscientes do original

1. **Painel de jogos**: o original desenha cada jogo como uma fileira de cartas
   em `FUN_1000_221f`; o porte usa uma linha compacta por jogo
   (`♣ 3 4 5 2♥ 7 … c+200`). Cabem 6 linhas por vez; ↑/↓ rolam a lista quando
   há mais jogos.
2. **IA**: heurística própria (não é a do original, que era guiada por
   perguntas ao jogador). Ela compra a mesa só quando a carta do topo fecha um
   jogo imediatamente (aumentando um jogo existente ou formando um novo, dentro
   do limite de slots); caso contrário compra do monte.
3. **Save**: `F2` grava um arquivo texto (`BURACO-SAVE 1`) com mãos, jogos,
   morto, monte, mesa, placar, estatísticas da partida, fase, jogador da vez e
   o estado do RNG, para a partida continuar exatamente do ponto gravado. `F5`
   restaura. O nome do arquivo é o nome do jogador (`<nome>.sav`), com
   numeração progressiva quando necessário; o fluxo inicial escolhe entre
   continuar o save mais recente da pasta ou criar um novo jogo.

## 8. Validação

* `ctest --test-dir build` roda os 131 casos de teste (cartas, máquina de
  turnos, save/load, nomes de arquivo, invariantes, tela, configuração de
  vídeo e som, cálculo e estado da janela, tela de opções, síntese e eventos
  sonoros). O caso de tela compara o hash das 25 linhas com a saída capturada
  de `--auto 400 --seed 1234`, então a divisão do antigo `main.cpp` em módulos
  não mudou um pixel.
* `shots/33_deal720.png` (original) vs. `shots/41_port_deal.png` (porte):
  mesma distribuição de linhas, mesma fan vertical de 2 colunas por carta.
* `--check 20000` fecha partidas inteiras nos seeds 99, 777, 12345, 4242, 7,
  2024, 31337, 555, 1, 2, 3, 42, 1000 e 65535, imprimindo `OK` sem cartas
  duplicadas ou perdidas (e sem estado repetido/travado).
* `--savetest 900` grava, recarrega e continua 400 fases em paralelo: o estado
  permanece idêntico nos oito seeds; `--savetest 2000` também passa no 31337.
  O teste compara ids físicos e flags de curinga, e re-serializa o estado
  carregado para garantir que o save não perde nenhum campo.
* `work/keys_buymesa2.txt` (`F5`, `S`): a mão vai de 11 para 14 cartas e a mesa
  de 3 para 0 — a compra leva o descarte inteiro.
* `work/keys_lay_new.txt` / `work/keys_lay_grow.txt`: com 6-7-8♥ na mão e um
  jogo 3-4-5♥ na mesa, `N` cria um segundo jogo de copas e `A` aumenta o
  existente.
* `work/x.txt` (`--script`) contém 5 mãos completas com placar, morto e
  canastras; `work/auto.txt` roda 4.000 fases sem travar.
* `--flow --script work/keys_flow_new.txt` exercita a tela inicial, a digitação
  do nome, o `F2` (cria `<nome>.sav` na pasta indicada por `--savedir`) e a
  tela cheia do `F3`; repetir o nome gera `<nome>2.sav` e o fluxo de continuar
  restaura o estado gravado.
* Capturas do porte: `shots/50_port_melds.png`, `shots/52_port_melds.png`,
  `shots/60_port_score.png` (tela de contagem detalhada).
