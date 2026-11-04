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
| Baixar jogo, aumentar jogo, vários jogos do mesmo naipe (até 4) | ✅ |
| Escolher entre aumentar um jogo existente ou baixar um novo do mesmo naipe | ✅ |
| Curingas (2 e joker) preenchendo buracos e topo | ✅ |
| Pegar o morto (11 + 11), penalidade de −100 | ✅ |
| Bater (200 baixando / 100 descartando), exigir canastra | ✅ |
| Contagem de pontos, canastra limpa +200 / suja +100 | ✅ |
| Partida até 3.000 pontos, várias mãos | ✅ |
| Sorteio diferente a cada nova partida | ✅ |
| Fonte VGA 8×16 autêntica extraída do DOSBox-X | ✅ |
| F1 ajuda, F3 situação; mão do adversário sempre oculta | ✅ |
| F2 salvar / F5 restaurar (`buraco.sav`) | ✅ |
| Modo headless para regressão/diff | ✅ |

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

## 3. Controles

| Tecla | Ação |
|---|---|
| ← / → | escolhe a carta sob o cursor (marcador `▼` na linha 17) |
| Espaço | marca/desmarca a carta para baixar |
| ENTER (fase compra) | compra do monte |
| S / N | responde "Comprar a mesa (S/N) ?" (S leva a mesa inteira) |
| ENTER (fase jogo) | baixa as cartas marcadas; sem marcação → vai para o descarte |
| A / N (pergunta) | aumenta o jogo existente (A) ou baixa um novo do mesmo naipe (N) |
| ENTER (fase descarte) | descarta a carta sob o cursor |
| ENTER (fim de mão) | próxima mão |
| F1 | tela de ajuda |
| F3 | situação geral (placar e número da mão) |
| F2 | grava a partida em `buraco.sav` |
| F5 | restaura a partida gravada |
| ESC | limpa a seleção; sem seleção, encerra |

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
```

`--save <arquivo>` troca o caminho padrão do save (útil para testes sem
sobrescrever o `buraco.sav` real). `--check` imprime uma linha por mão fechada
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
src/main.cpp   camada de plataforma dos::* + apresentação ui::* + main()
src/cards.*    modelo de carta, validação de jogo, valor de canastra
src/game.*     máquina de turnos, IA, contagem de pontos, partida
assets/        font8x16.bin (256 glifos × 16 bytes, VGA 8×16)
```

### `dos::` — o que foi portado do BIOS/DOS

| Serviço original | Substituto |
|---|---|
| `INT 10h` (vídeo) | `dos::Video` + `dos::int10` |
| `INT 16h` (teclado) | `dos::Keyboard` + `dos::int16` |
| `INT 33h` (mouse) | `dos::Mouse` + `dos::int33` |
| ticks do PIT / `INT 1Ah` | `dos::Clock` (18,2065 Hz) |
| DAC VGA (`3C8h/3C9h`) | `Video::setDac` / `dacWrite` |
| polling de retraço (`3DAh`) | `dos::Ports::in` |
| VRAM de texto `B800:0000` | `Video::text` (80×25×2) |
| VRAM modo 13h `A000:0000` | `Video::vga` (320×200) |

Foi mantido o *framebuffer-first shim*: o código de jogo escreve nos mesmos
buffers virtuais que o original escrevia e a renderização é uma camada
separada e substituível.

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
  internos e/ou estendem o topo. Cada jogador pode ter até 4 jogos, inclusive
  mais de um do mesmo naipe (o novo jogo também precisa de ≥3 cartas).
* Comprar a mesa leva **todas** as cartas do descarte, não só a última; a
  última carta comprada vira `lastDrawn_` e é a primeira que a IA tenta baixar.
* Canastra: ≥7 cartas → +200 limpa, +100 suja (com curinga).
* Bater: mão vazia com canastra; +200 se baixou tudo na fase de jogos, +100 se
  foi no descarte (confirmado em `FUN_1000_30bf`: `0xBF0 = 1`, bônus 200 quando
  `0x27B == 2`, senão 100).
* Morto: 22 cartas; quem esvazia a mão pega 11; quem nunca pegou perde 100.
* Monte vazio: o original sorteia a carta 0 e o jogador não compra nada
  (`FUN_1000_2a03` devolve 0 quando os 108 slots estão vazios) — a mão encolhe
  a cada descarte até alguém bater.
* Partida até 3.000 pontos.

### Save (`F2`/`F5`)

Arquivo texto `BURACO-SAVE 1` (padrão `buraco.sav`, ou `--save <arquivo>`).
Guarda fase, jogador da vez, semente do RNG, mãos, jogos (15 slots + flags de
curinga), monte, mesa, morto, placar e a mensagem. O load valida os ids
(1..108) e os contadores antes de substituir o estado; um arquivo corrompido é
recusado por inteiro, sem quebrar a partida em andamento.

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
   (`♣ 3 4 5 2♥ 7 … c+200`) para caber até quatro jogos sem rolagem.
2. **IA**: heurística própria (não é a do original, que era guiada por
   perguntas ao jogador). Ela compra a mesa só quando a carta do topo fecha um
   jogo imediatamente (aumentando um jogo existente ou formando um novo, dentro
   do limite de quatro); caso contrário compra do monte.
3. **Save**: `F2` grava um arquivo texto (`BURACO-SAVE 1`) com mãos, jogos,
   morto, monte, mesa, placar, fase, jogador da vez e o estado do RNG, para a
   partida continuar exatamente do ponto gravado. `F5` restaura.

## 8. Validação

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
* Capturas do porte: `shots/50_port_melds.png`, `shots/52_port_melds.png`.
