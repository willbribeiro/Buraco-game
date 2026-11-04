#!/usr/bin/env python3
"""PKLITE 1.14 "small" mode decompressor, derived from the game's own stub.

The stub (file 0x70..0x239) is copied to paragraph 0x193F by the MZ loader
prologue and decompresses into that same segment starting at offset 0x100.

Relevant tables embedded in the relocated core (file offsets):
    0x213..0x222  length/count table  (CS:0x161)
    0x223..0x238  offset-high table   (CS:0x171)

Control flow reproduced from the disassembly at PSP-relative 0x1A7..0x225.
"""
from __future__ import annotations

import sys
from pathlib import Path

T1 = [0x03, 0x00, 0x02, 0x0A, 0x04, 0x05, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x06, 0x07, 0x08, 0x09]
T2 = [0x01, 0x02, 0x00, 0x00, 0x03, 0x04, 0x05, 0x06,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D]


class Stream:
    __slots__ = ("d", "si", "chunk", "left")

    def __init__(self, data: bytes, start: int):
        self.d = data
        self.chunk = data[start] | (data[start + 1] << 8)
        self.si = start + 2
        self.left = 16

    def bit(self) -> int:
        b = self.chunk & 1
        self.chunk >>= 1
        self.left -= 1
        if self.left == 0:
            self.chunk = self.d[self.si] | (self.d[self.si + 1] << 8)
            self.si += 2
            self.left = 16
        return b

    def byte(self) -> int:
        v = self.d[self.si]
        self.si += 1
        return v


def decode_count(s: Stream) -> tuple[int, bool]:
    bx = s.bit()
    bx = (bx << 1) | s.bit()
    if bx != 0:
        bx = (bx << 1) | s.bit()
        if bx >= 6:
            bx = (bx << 1) | s.bit()
    count = T1[bx] if bx < len(T1) else 0
    if count == 0x0A:
        b = s.byte()
        if b == 0xFF:
            return 0, True
        count = 10 + b
    return count, False


def decode_high(s: Stream) -> int:
    if s.bit() == 0:
        return 0
    bx = s.bit()
    bx = (bx << 1) | s.bit()
    bx = (bx << 1) | s.bit()
    if bx >= 2:
        bx = (bx << 1) | s.bit()
        if bx >= 8:
            bx = (bx << 1) | s.bit()
            if bx >= 0x17:
                bx = (bx << 1) | s.bit()
                return bx & 0xDF
            return T2[bx]
        return T2[bx]
    return T2[bx]


def decompress(data: bytes, start: int, limit: int = 1 << 20) -> bytes:
    s = Stream(data, start)
    out = bytearray()
    while len(out) < limit:
        if s.bit() == 0:
            out.append(s.byte())
            continue
        count, done = decode_count(s)
        if done:
            return bytes(out)
        high = decode_high(s) if count != 2 else 0
        off = (high << 8) | s.byte()
        src = len(out) - off
        if src < 0:
            raise ValueError(f"offset {off} before start at {len(out)}")
        for _ in range(count):
            out.append(out[src])
            src += 1
    raise ValueError("no end marker")


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("BURACO.EXE")
    data = src.read_bytes()
    start = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x240
    out = decompress(data, start)
    text = out.decode("cp437", "replace")
    marks = [m for m in ("Embaralhando", "Compre", "Por favor", "pontos") if m in text]
    print(f"0x{start:04X}: {len(out)} bytes, marks={marks}")
    dst = Path(sys.argv[3]) if len(sys.argv) > 3 else src.with_suffix(".unp.bin")
    dst.write_bytes(out)
    print("->", dst)


if __name__ == "__main__":
    main()
