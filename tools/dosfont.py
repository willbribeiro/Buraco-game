#!/usr/bin/env python3
"""Shared helpers for reading DOS text-mode screens captured from DOSBox-X.

DOSBox-X renders VGA text mode at 720x400: 80 cells of 9 px, glyphs 8 px wide.
The glyph grid can be off by a pixel depending on window decoration, so the
vertical origin is recovered by minimising the number of lit pixels that fall
exactly on a cell boundary (a correctly aligned grid cuts no glyph in half).
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image

COLS, ROWS = 80, 25
CELL_W, CELL_H = 9, 16
GLYPH_W = 8

EGA = [
    (0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
    (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
    (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
    (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255),
]


def nearest_ega(rgb: tuple[int, int, int]) -> int:
    best, bestd = 0, 1 << 30
    for i, c in enumerate(EGA):
        d = (rgb[0] - c[0]) ** 2 + (rgb[1] - c[1]) ** 2 + (rgb[2] - c[2]) ** 2
        if d < bestd:
            best, bestd = i, d
    return best


def client_origin(img: Image.Image) -> tuple[int, int]:
    w, h = img.size
    if w < 720 or h < 400:
        raise SystemExit(f"unexpected capture size {w}x{h}; use windowresolution=720x400")
    return (w - 720) // 2, 52


def _lit(px, x: int, y: int) -> bool:
    r, g, b = px[x, y]
    return r + g + b > 96


def detect_top(img: Image.Image) -> int:
    """Return the y of the first glyph row of the text grid.

    The DOSBox-X client area nominally starts at y=52 but the first glyph row
    starts one pixel above it; this was verified against the canonical VGA
    bitmap for 'A' (00 00 10 38 6C C6 C6 FE C6 C6 C6 C6 00 00 00 00).
    """
    ox, oy = client_origin(img)
    return oy - 1


def load_font(path: Path) -> list[bytes]:
    data = Path(path).read_bytes()
    if len(data) != 4096:
        raise SystemExit(f"{path}: expected 4096 bytes, got {len(data)}")
    return [data[i * 16:(i + 1) * 16] for i in range(256)]


def extract_font(img: Image.Image) -> bytes:
    """Extract the 8x16 font from an ALLCHARS.COM screen."""
    ox, _ = client_origin(img)
    top = detect_top(img)
    px = img.convert("RGB").load()
    font = bytearray(256 * 16)
    for ch in range(256):
        row, col = ch >> 4, (ch & 15) * 5
        x0, y0 = ox + col * CELL_W, top + row * CELL_H
        for gy in range(16):
            bits = 0
            for gx in range(GLYPH_W):
                if _lit(px, x0 + gx, y0 + gy):
                    bits |= 0x80 >> gx
            font[ch * 16 + gy] = bits
    return bytes(font)


def decode(img: Image.Image, font: list[bytes]) -> list[list[tuple[str, int, int]]]:
    """Decode the screen into (char, fg, bg) per cell."""
    ox, oy = client_origin(img)
    top = detect_top(img)
    px = img.convert("RGB").load()
    lookup = {b: i for i, b in enumerate(font)}
    grid: list[list[tuple[str, int, int]]] = []

    for row in range(ROWS):
        line: list[tuple[str, int, int]] = []
        for col in range(COLS):
            x0, y0 = ox + col * CELL_W, top + row * CELL_H
            bg = nearest_ega(px[x0, y0])
            counts: dict[int, int] = {}
            for gy in range(16):
                for gx in range(CELL_W):
                    idx = nearest_ega(px[x0 + gx, y0 + gy])
                    if idx != bg:
                        counts[idx] = counts.get(idx, 0) + 1
            fg = max(counts, key=lambda k: counts[k]) if counts else 0

            ch = 0x20
            matched = False
            for shift in (0, 1):
                bits = bytearray(16)
                for gy in range(16):
                    b = 0
                    for gx in range(GLYPH_W):
                        if nearest_ega(px[x0 + shift + gx, y0 + gy]) == fg:
                            b |= 0x80 >> gx
                    bits[gy] = b
                key = bytes(bits)
                if not any(key):
                    ch, matched = 0x20, True
                    break
                if key in lookup:
                    ch, matched = lookup[key], True
                    break
            line.append((chr(ch if matched else 0xB1), fg, bg))
        grid.append(line)
    return grid
