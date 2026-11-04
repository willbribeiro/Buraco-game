#!/usr/bin/env python3
"""Static PKLITE 1.x decompressor.

Port of the bit trees from OpenTESArena's ExeUnpacker / hackerb9's depklite.
The LZ stream is a 16-bit little-endian bit array followed by raw literal
bytes and Huffman-coded (length, offset) copy tokens.

In the original tables a node's child is a *relative* offset (`node += n`),
so the tables below are converted to absolute indices at build time.
"""
from __future__ import annotations

import sys
from pathlib import Path

# (left_offset, right_offset) for internal nodes, int for leaves.
T1_FLAT = [
    (4, 1),
    (1, 2), 2, 3,
    (1, 6),
    (1, 2), 4,
    (1, 2), 5, 6,
    (1, 6),
    (1, 2), 7,
    (1, 2), 8, 9,
    (1, 6),
    (1, 2), 10,
    (1, 2), 11, 12,
    (1, 6),
    (1, 2), 25,
    (1, 2), 13, 14,
    (1, 6),
    (1, 2), 15,
    (1, 2), 16, 17,
    (1, 6),
    (1, 2), 18,
    (1, 2), 19, 20,
    (1, 4),
    (1, 2), 21, 22,
    (1, 2), 23, 24,
]

T2_FLAT = [
    (2, 1), 0,
    (1, 12),
    (1, 4),
    (1, 2), 1, 2,
    (1, 4),
    (1, 2), 3, 4,
    (1, 2), 5, 6,
    (1, 18),
    (1, 8),
    (1, 4),
    (1, 2), 7, 8,
    (1, 2), 9, 10,
    (1, 4),
    (1, 2), 11, 12,
    (1, 2), 13,
    (1, 2), 14, 15,
    (1, 16),
    (1, 8),
    (1, 4),
    (1, 2), 16, 17,
    (1, 2), 18, 19,
    (1, 4),
    (1, 2), 20, 21,
    (1, 2), 22, 23,
    (1, 8),
    (1, 4),
    (1, 2), 24, 25,
    (1, 2), 26, 27,
    (1, 4),
    (1, 2), 28, 29,
    (1, 2), 30, 31,
]


def build(flat):
    nodes = []
    for i, e in enumerate(flat):
        if isinstance(e, int):
            nodes.append((0, 0, e))
        else:
            left, right = e
            nodes.append((i + left if left else 0, i + right if right else 0, -1))
    return nodes


T1 = build(T1_FLAT)
T2 = build(T2_FLAT)


def sanity(tree, name):
    leaves = {n[2] for n in tree if n[0] == 0 and n[1] == 0}
    assert leaves == set(range(0, 32)) - ({25} if name == "t2" else set()), (name, sorted(leaves))


class BitStream:
    __slots__ = ("d", "p", "bits", "nbits")

    def __init__(self, data: bytes, pos: int):
        self.d = data
        self.bits = data[pos] | (data[pos + 1] << 8)
        self.p = pos + 2
        self.nbits = 0

    def bit(self) -> bool:
        b = (self.bits >> self.nbits) & 1
        self.nbits += 1
        if self.nbits == 16:
            self.nbits = 0
            self.bits = self.d[self.p] | (self.d[self.p + 1] << 8)
            self.p += 2
        return bool(b)

    def byte(self) -> int:
        v = self.d[self.p]
        self.p += 1
        return v


def tree_get(nodes, bs: BitStream) -> int:
    i = 0
    while True:
        i = nodes[i][1] if bs.bit() else nodes[i][0]
        if nodes[i][0] == 0 and nodes[i][1] == 0:
            return nodes[i][2]


def decompress(data: bytes, start: int, limit: int = 4 << 20) -> bytes:
    bs = BitStream(data, start)
    out = bytearray()
    while len(out) < limit:
        if not bs.bit():
            out.append(bs.byte())
            continue
        copy = tree_get(T1, bs)
        if copy == 25:
            b = bs.byte()
            if b == 0xFE:
                continue
            if b == 0xFF:
                return bytes(out)
            count = b + 25
        else:
            count = copy
        high = tree_get(T2, bs) if count != 2 else 0
        off = bs.byte() | (high << 8)
        src = len(out) - off
        if src < 0:
            raise ValueError(f"copy offset {off} before start ({len(out)} bytes out)")
        for _ in range(count):
            out.append(out[src])
            src += 1
    raise ValueError("no end marker within limit")


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("BURACO.EXE")
    data = src.read_bytes()
    starts = [int(a, 0) for a in sys.argv[2:]] or [0x240]
    for start in starts:
        try:
            out = decompress(data, start)
        except Exception as e:  # noqa: BLE001
            print(f"0x{start:04X}: FAIL {e}")
            continue
        text = out.decode("cp437", "replace")
        marks = [m for m in ("Embaralhando", "Compre", "Por favor", "pontos") if m in text]
        print(f"0x{start:04X}: {len(out)} bytes marks={marks}")
        dst = src.with_suffix(f".{start:04X}.bin")
        dst.write_bytes(out)
        print("  ->", dst)


if __name__ == "__main__":
    main()
