#!/usr/bin/env python3
"""Decode an 80x25 DOS text-mode screen capture into characters + attributes.

Usage:
    python tools/read_screen.py shots/xx.png            # print text
    python tools/read_screen.py shots/xx.png --attrs    # print attr map
    python tools/read_screen.py shots/xx.png --json out.json
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dosfont  # noqa: E402


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    src = Path(args[0] if args else "shots/30_table720.png")
    font_path = Path("port/assets/font8x16.bin")
    if not font_path.exists():
        raise SystemExit("run tools/extract_font.py first")
    font = dosfont.load_font(font_path)
    grid = dosfont.decode(Image.open(src), font)

    if "--json" in sys.argv:
        out = Path(sys.argv[sys.argv.index("--json") + 1])
        out.write_text(json.dumps({
            "size": [dosfont.COLS, dosfont.ROWS],
            "text": ["".join(c for c, _, _ in row) for row in grid],
            "fg": [[f for _, f, _ in row] for row in grid],
            "bg": [[b for _, _, b in row] for row in grid],
        }, indent=1), encoding="utf-8")
        print(f"wrote {out}")
        return

    if "--attrs" in sys.argv:
        for r, row in enumerate(grid):
            print(f"{r:2d} " + " ".join(f"{(b << 4) | f:02X}" for _, f, b in row))
        return

    print(f"--- {src} ---")
    for r, row in enumerate(grid):
        print(f"{r:2d}|{''.join(c for c, _, _ in row).rstrip()}")


if __name__ == "__main__":
    main()
