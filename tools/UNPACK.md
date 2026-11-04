# Unpacking BURACO.EXE (PKLITE 1.x) with DOSBox-X

`BURACO.EXE` is a 16-bit real-mode MZ compressed with **PKLITE 1.x**
(`PKLITE Copr. 1990-92 PKWARE Inc.` at file offset `0x1E`). Ghidra and REA see
only the decompressor stub until the image is unpacked in memory.

## 1. Start the debugger

```powershell
C:\DOSBox-X\dosbox-x.exe -conf tools\dosbox-debug.conf -debug
```

At the debugger prompt:

```
BPDEL *
BPINT 10 00
RUN
```

`BPINT 10 00` stops on the first `INT 10h, AH=00h` (set video mode). PKLITE
does not call the video BIOS, so by the time this breakpoint fires the
original image is decompressed and the stub has already jumped to the OEP.

If the game sets the mode through VGA ports instead, just `RUN`, wait for the
first screen, then break with the debugger hotkey (Alt+Pause by default).

## 2. Record the runtime state

```
R
DOS MCBS
```

`R` shows the current `CS:IP` and `SS:SP`. `DOS MCBS` shows the PSP and the
allocated block; the load module starts at `PSP + 0x10`.

## 3. Dump memory

```
MEMDUMPBIN 0 100000 mem.bin
```

This writes the first 1 MiB of linear RAM, which includes the PSP, the
unpacked program, the BIOS data area and video memory. Import it in Ghidra as:

- Format: **Raw Binary**
- Language: **x86:LE:16:Real Mode**
- Base address: `0x0`
- Entry point: `CS * 16 + IP` (from `R`)

All real-mode `segment:offset` references then resolve to their real linear
addresses.

If you prefer a per-program image, dump only the block:

```
MEMDUMPBIN <load_seg>:0 <bytes> buraco_load.bin
```

and import at base `load_seg * 16`.

## 4. Capture the font and the first screen

```
VGA FONTDUMP C:\Users\ventoy\Documents\Buraco\port\assets\font8x16.bin
MEMDUMPBIN B800:0 4000 text_screen.bin
```

`text_screen.bin` is 80×25×2 bytes: character, attribute. It is the exact
rendered screen and is often a better reference than the game's internal
drawing code.

## 5. If the game uses overlays

`DOS MCBS` shows every block. Dump each block that contains code/data; overlay
managers load them on demand, so break on `INT 21h AH=3Fh` and dump the buffer
after each read.
