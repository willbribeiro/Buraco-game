# Buraco - Port to SDL2

**Buraco** é uma implementação moderna em C++17 do clássico jogo de cartas MS-DOS "Buraco", portado para rodar em ambientes modernos usando **SDL2**. O projeto foi desenvolvido a partir da desmontagem e análise forense do executável original `BURACO.EXE`, recuperando o código-fonte da lógica de negócios e implementando uma nova camada de plataforma.

## 📌 Sobre o Projeto

Este projeto é um exemplo notável de **reverse engineering moderno**:

- O jogo original (`BURACO.EXE`) foi descompactado usando a ferramenta `UNP.EXE` (PKLITE)
- A lógica do jogo (em C) foi identificada, descompilada e reimplementada em C++17
- Uma camada de plataforma SDL2 substitui os serviços BIOS/DOS originais:
  - **INT 10h** → Vídeo virtual via framebuffer
  - **INT 16h** → Teclado com código de varredura real
  - **INT 33h** → Mouse Microsoft virtualizado
  - **PIT timer** → Timer virtual a 18.2065 Hz

## ✨ Recursos

- ✅ Jogo completo e funcional (ponto-e-ponto da lógica original)
- 🎨 Renderização SDL2 com framebuffer virtual
- ⌨️ Input de teclado com mapeamento BIOS
- 🖱️ Suporte a mouse virtual
- 📜 Tela de ajuda F1, situação geral F3, gravar F2, restaurar F5
- 💾 Sistema de saves/carregamentos
- 🤖 Modo headless para testes automatizados (`--script`, `--savetest`)
- 🧪 Verificação de invariantes do jogo (accounting de 108 cartas)

## 🏗️ Estrutura do Projeto

```
Buraco/
├── port/                      # Camada de plataforma SDL2
│   ├── src/
│   │   ├── main.cpp          # Entry point, vídeo, teclado, mouse
│   │   ├── game.h            # Lógica do jogo (cabeçalho)
│   │   ├── game.cpp          # Implementação da lógica (ponto-e-ponto original)
│   │   └── cards.h           # Estrutura de dados das cartas
│   └── assets/
│       └── font8x16.bin      # Fonte 8x16 extraída do jogo original
├── tools/                     # Ferramentas auxiliares
│   ├── UNPACK.md             # Instruções para descompactar e depurar
│   ├── read_screen.py        # Captura da tela via OCR
│   ├── probe_vrt.py          # Probes de vídeo virtual
│   └── ...                   # Vários scripts auxiliares
├── work/                      # Arquivos gerados durante o processo
├── shots/                     # Screenshots das telas do jogo
└── README.md                 # Este arquivo
```

## 🚀 Requisitos

- **CMake** 3.14+
- **vcpkg** (para instalar SDL2)
- **MSVC** ou **GCC/Clang** com C++17 suportado
- SDL2 (via vcpkg ou sistema)

## 🔧 Instalação e Compilação

### 1. Configurar vcpkg

```powershell
# Instalar o vcpkg se ainda não tiver
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

# Instalar SDL2
.\vcpkg\install-x64-windows.bat sdl2:x64-windows
```

### 2. Compilar

```powershell
cd C:\Users\ventoy\Documents\Buraco\port
.\build.ps1
```

O script `build.ps1` configura o CMake e gera os projetos para VS2022 ou Makefile (Linux).

### 3. Executar

```powershell
# Janela SDL2
.\port\build\buraco.exe

# Modo headless (teste)
.\port\build\buraco.exe --script tools\auto12345.txt --out work\screen.txt

# Teste de save/restore
.\port\build\buraco.exe --savetest 10
```

## 📜 Como Jogar

### Regras Básicas (Buraco / Burak)

- Objetivo: chegar a **500 pontos** primeiro
- Cada jogador começa com **26 cartas** distribuídas de uma baralhadilha de 108 cartas
- Forme "jogos" (melds) na mesa para pontuar:
  - **Trinta**: 3 ou mais do mesmo naipe em sequência (ex: 7♦, 8♦, 9♦)
  - **Canasta** (cancha): 4+ cartas do mesmo naipe = +100 pontos extras
- Use as setas para navegar entre suas cartas, ESPAÇO para confirmar, ENTER para descartar/concluir

### Controles

| Tecla | Ação |
|-------|------|
| ← → | Navegar entre suas cartas |
| ↑ ↓ | Ver jogos na mesa / alternar ajuda/estado |
| ESPAÇO | Confirmar seleção de carta (baixar jogo) |
| ENTER | Descartar carta / finalizar turno |
| Y | Aceitar "aumentar" um jogo existente |
| N | Recusar |
| F1 | Abrir tela de ajuda |
| F2 | Gravar jogo (save) |
| F3 | Ver situação geral |
| F5 | Restaurar jogo do save |
| ESC | Sair / cancelar |

## 🛠️ Ferramentas Incluídas

### `UNPACK.EXE` e `BURACO_UNPACKED.EXE`

Arquivos descompactados do executável original. O primeiro contém o stub de decompressão; o segundo já está expandido, permitindo análise em Ghidra ou REA.

### Scripts Python (tools/)

- `read_screen.py` - Captura e processa a tela
- `probe_vrt.py` - Debug virtual
- `capture_game.py` - Gravação automática do jogo
- `unpack_run.py` - Descompactação assistida

### Ghidra Project (`work/ghidra/buraco.gpr`)

Arquivo de projeto Ghidra com a descompilação da lógica original.

## 📚 Documentação Técnica

Veja [`tools/UNPACK.md`](tools/UNPACK.md) para:

1. Como iniciar o debugger e capturar memória
2. Dump do vídeo e fonte original
3. Análise de overlays e MCBS
4. Importação no Ghidra (x86 LE 16-bit Real Mode)

## 🧪 Testes Automatizados

O projeto inclui um modo headless para testes regressivos:

```powershell
# Reproduz um script de teclas pré-gravado
.\port\build\buraco.exe --script tools\auto12345.txt --out work\screen.txt

# Faz 10 turnos automáticos e guarda o estado
.\port\build\buraco.exe --auto 10

# Valida save/restore com check de invariantes
.\port\build\buraco.exe --savetest 10 --check 5
```

Cada tela é salva como `screen.txt` (ASCII 80x25), e um estado textual (`*.sav`) é anexado para verificação sem OCR.

## 📸 Screenshots

| Imagem | Descrição |
|--------|-----------|
| [`shots/01_boot.png`](shots/01_boot.png) | Tela inicial |
| [`shots/03_screen.png`](shots/03_screen.png) | Primeira tela do jogo |
| [`shots/10_game.png`](shots/10_game.png) | Jogo em andamento (split screen) |
| [`shots/20_chars.png`](shots/20_chars.png) | Caras e cores das cartas |
| [`shots/54_port_final.png`](shots/54_port_final.png) | Tela final de jogo |

## 🔍 Notas de Arquivo

A camada de plataforma (`port/src/main.cpp`) foi escrita para:

- Manter a convenção de registradores da chamada DOS 16-bit (`AX`, `BX`, etc.)
- Permitir tradução direta de código descompilado (Ghidra → C++ SDL2)
- Garantir que cada chamada à BIOS tenha um emulador virtual com a mesma assinatura

## 📄 Licença

Este projeto é distribuído sob **MIT License**. Verifique o arquivo [`LICENSE`](LICENSE) para detalhes.

## 👤 Contribuidores

- Desenvolvido durante experimentação de reverse engineering moderno
- Baseado no executável original `BURACO.EXE` (PKLITE 1.x, PKWARE Inc.)

---

**Em suma:** Buraco é um clássico dos anos 90 que ganhou nova vida com SDL2, mantendo toda a lógica original e adicionando modernas práticas de C++. 🎮✨
