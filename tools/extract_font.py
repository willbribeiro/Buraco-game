#!/usr/bin/env python3
"""Extract the 8x16 CP437 VGA text font from an ALLCHARS.COM screen capture.

The DOS helper programme draws every byte value 0..255 at a known grid
position (row = c >> 4, column = (c & 15) * 5) using the video BIOS in
80x25 text mode.  Capturing the DOSBox-X window at 720x400 makes each
character cell 9x16 pixels and the glyph exactly 8 pixels wide, so the
glyph bitmaps can be copied out without any scaling.

Usage:  python tools/extract_font.py shots/22_chars720.png port/assets/font8x16.bin
"""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dosfont  # noqa: E402


def main() -> None:
    src = Path(sys.argv[1] if len(sys.argv) > 1 else "shots/22_chars720.png")
    dst = Path(sys.argv[2] if len(sys.argv) > 2 else "port/assets/font8x16.bin")
    img = Image.open(src)
    font = dosfont.extract_font(img)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(font)

    top = dosfont.detect_top(img)
    blank = [c for c in range(32, 256) if not any(font[c * 16:c * 16 + 16])]
    print(f"{src} -> {dst} ({len(font)} bytes, glyph grid top y={top})")
    print(f"blank printable glyphs: {len(blank)}")
    print("'A' = " + " ".join(f"{b:02X}" for b in font[0x41 * 16:0x41 * 16 + 16]))


if __name__ == "__main__":
    main()
