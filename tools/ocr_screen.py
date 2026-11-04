#!/usr/bin/env python3
"""Decode an 80x25 DOS text-mode screenshot into a character grid.

The DOSBox-X window is captured with PrintWindow; the text area is located by
its uniform background colour, then split into 80x25 cells.  Glyph templates are
learned from a screen whose text is already known (the game's intro/help page),
so no external font file is required.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

from PIL import Image

W, H = 80, 25

INTRO = {
    2: " Vamos jogar buraco ?",
    4: ' Durante o jogo, voce pode responder minhas perguntas com "S" ou "N".',
    5: " As flechas para esquerda e direita servem para escolher as suas cartas.",
    6: " Quando voce tiver escolhido, aperte a barra de espacos para que eu",
    7: " saiba qual carta voce escolheu. Se voce nao quizer mais esta carta,",
    8: " aperte a barra novamente. Para baixar um jogo, escolha algumas cartas",
    9: " e no fim aperte ENTER. Para descartar, aperte ENTER sem ter escolhido",
    10: " nenhuma carta.",
    12: " As flechas para cima e para baixo mostram os jogos que estao na mesa.",
    13: " Para ver meus jogos, aperte PageUp.",
    14: " Para ver esta tela, aperte F1.",
    15: " Para gravar o jogo e continuar depois, aperte F2.",
    16: " Para ver a situacao geral do jogo, aperte F3.",
    17: " Se houver algum problema na tela, aperte F4 e tudo sera redesenhado.",
    19: " Boa sorte!",
    22: "                 ...agora aperte ENTER",
}


EXTRA = [("11_name.png", 0), ("14_deal.png", 0)]
EXTRA_TEXTS = {
    "11_name.png": {0: "Seu nome (8 letras): "},
    "14_deal.png": {
        0: "Compre uma carta do monte. <ENTER>",
        15: "Sua mao.",
    },
}


def find_text_area(img: Image.Image) -> tuple[int, int, int, int]:
    """Return (x0, y0, x1, y1) of the emulated screen inside the window.

    PrintWindow on the DOSBox-X main window returns an 800x600 client area
    framed by 8 px side borders, a 52 px title/menu strip and a 7 px bottom
    border, so the geometry is known for this capture setup.
    """
    w, h = img.size
    if (w, h) == (816, 659):
        return 8, 52, 808, 652
    px = img.convert("RGB").load()
    from collections import Counter

    counts = Counter(px[x, y] for y in range(h) for x in range(w))
    bg = counts.most_common(1)[0][0]
    xs = [x for x in range(w) if px[x, h // 2] == bg]
    ys = [y for y in range(h) if px[w // 2, y] == bg]
    # the emulated area is the widest contiguous run of the background colour
    def longest_run(vals):
        best = (0, 0)
        start = prev = None
        for v in vals:
            if prev is not None and v == prev + 1:
                prev = v
            else:
                start = prev = v
            if prev - start > best[1] - best[0]:
                best = (start, prev)
        return best

    x0, x1 = longest_run(xs)
    y0, y1 = longest_run(ys)
    return x0, y0, x1 + 1, y1 + 1


def cell_bitmap(img: Image.Image, box, cw: int, ch: int, col: int, row: int):
    """Sample one cell into a 8x16 boolean grid (True = foreground)."""
    cw, ch = int(cw), int(ch)
    x0, y0 = box[0] + col * cw, box[1] + row * ch
    px = img.load()
    # background = most common colour in the cell
    from collections import Counter

    samples = [px[x0 + x, y0 + y] for y in range(ch) for x in range(cw)]
    bg = Counter(samples).most_common(1)[0][0]
    bsum = sum(bg)
    bits = []
    for gy in range(16):
        for gx in range(8):
            sx = x0 + int((gx + 0.5) * cw / 8)
            sy = y0 + int((gy + 0.5) * ch / 16)
            bits.append(1 if sum(px[sx, sy]) > bsum + 60 else 0)
    return bits


def learn(path: Path) -> tuple[dict, tuple, int, int]:
    img = Image.open(path).convert("RGB")
    box = find_text_area(img)
    cw = int((box[2] - box[0]) / W)
    ch = int((box[3] - box[1]) / H)
    templates: dict = {}

    def add(image, rows, offset):
        for row, text in rows.items():
            for col, chx in enumerate(" " * offset + text):
                bits = cell_bitmap(img if image is None else image, box, cw, ch, col, row)
                key = "".join(str(b) for b in bits)
                templates.setdefault(key, chx)

    add(None, INTRO, 2)
    for extra, offset in EXTRA:
        p = path.parent / extra
        if p.exists():
            add(Image.open(p).convert("RGB"), EXTRA_TEXTS[extra], offset)
    return templates, box, cw, ch


def decode(path: Path, templates: dict, box, cw: int, ch: int) -> list[str]:
    img = Image.open(path).convert("RGB")
    bg = img.getpixel((box[0] + 2, box[1] + 2))
    lines = []
    for row in range(H):
        line = []
        for col in range(W):
            bits = cell_bitmap(img, box, cw, ch, col, row)
            key = "".join(str(b) for b in bits)
            line.append(templates.get(key, "?"))
        lines.append("".join(line).rstrip())
    return lines


def main():
    root = Path(__file__).resolve().parent.parent
    shots = root / "shots"
    templates, box, cw, ch = learn(shots / "10_game.png")
    cache = root / "work" / "font_templates.json"
    cache.write_text(json.dumps({k: v for k, v in templates.items()}, ensure_ascii=False))
    print(f"area={box} cell={cw:.3f}x{ch:.3f} templates={len(templates)}")
    for name in sys.argv[1:] or ["10_game.png", "11_name.png", "12_table.png", "14_deal.png"]:
        p = shots / name
        if not p.exists():
            continue
        print(f"===== {name} =====")
        for i, line in enumerate(decode(p, templates, box, cw, ch)):
            print(f"{i:02d}|{line}")


if __name__ == "__main__":
    main()
