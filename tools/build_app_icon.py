#!/usr/bin/env python3
"""Generate the multi-resolution After Dark Studio AD monogram icon."""

from __future__ import annotations

import argparse
import binascii
import struct
import zlib
from pathlib import Path


LETTER_A = ("01110", "10001", "10001", "11111", "10001", "10001", "10001")
LETTER_D = ("11110", "10001", "10001", "10001", "10001", "10001", "11110")


def render(size: int) -> bytes:
    pixels = bytearray(size * size * 4)
    radius = max(2, size // 8)
    rim = max(1, size // 32)

    def set_pixel(x: int, y: int, color: tuple[int, int, int, int]) -> None:
        offset = (y * size + x) * 4
        pixels[offset : offset + 4] = bytes(color)

    for y in range(size):
        for x in range(size):
            edge_x = min(x, size - 1 - x)
            edge_y = min(y, size - 1 - y)
            corner_x = max(0, radius - edge_x)
            corner_y = max(0, radius - edge_y)
            if corner_x * corner_x + corner_y * corner_y > radius * radius:
                continue
            on_rim = edge_x < rim or edge_y < rim
            set_pixel(x, y, (0xF2, 0xB3, 0x3D, 0xFF) if on_rim else
                              (0x1B, 0x1D, 0x27, 0xFF))

    cell = max(1, size // 16)
    glyph_width = 5 * cell
    gap = cell
    left = (size - glyph_width * 2 - gap) // 2
    top = (size - 7 * cell) // 2

    def letter(rows: tuple[str, ...], x_origin: int,
               color: tuple[int, int, int, int]) -> None:
        for row, pattern in enumerate(rows):
            for column, enabled in enumerate(pattern):
                if enabled != "1":
                    continue
                for dy in range(cell):
                    for dx in range(cell):
                        set_pixel(x_origin + column * cell + dx,
                                  top + row * cell + dy, color)

    letter(LETTER_A, left, (0xF2, 0xB3, 0x3D, 0xFF))
    letter(LETTER_D, left + glyph_width + gap, (0xF4, 0xF4, 0xF6, 0xFF))
    return bytes(pixels)


def png(size: int) -> bytes:
    pixels = render(size)
    rows = b"".join(
        b"\0" + pixels[y * size * 4 : (y + 1) * size * 4]
        for y in range(size)
    )

    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
            chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def icon(sizes: tuple[int, ...]) -> bytes:
    images = [png(size) for size in sizes]
    offset = 6 + 16 * len(images)
    entries = bytearray()
    for size, image in zip(sizes, images, strict=True):
        dimension = 0 if size == 256 else size
        entries.extend(struct.pack("<BBBBHHII", dimension, dimension, 0, 0,
                                   1, 32, len(image), offset))
        offset += len(image)
    return struct.pack("<HHH", 0, 1, len(images)) + entries + b"".join(images)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(icon((16, 20, 24, 32, 40, 48, 64, 128, 256)))


if __name__ == "__main__":
    main()