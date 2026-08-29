#!/usr/bin/env python3
"""Generate independent Mandelbrot control and version resources."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def title_field(title: str) -> bytes:
    encoded = title.encode("latin-1")[:14]
    return encoded + bytes(14 - len(encoded))


def string_slider(title: str, labels: list[str], bounds: list[int], start: int) -> bytes:
    blob = bytearray(32 + 16 * len(labels) + 2 * len(bounds))
    struct.pack_into("<H", blob, 0, 1)
    blob[2:16] = title_field(title)
    struct.pack_into("<h", blob, 22, len(labels))
    struct.pack_into("<h", blob, 24, start)
    for index, label in enumerate(labels):
        encoded = label.encode("latin-1")[:15]
        offset = 32 + 16 * index
        blob[offset : offset + len(encoded)] = encoded
    for index, bound in enumerate(bounds):
        struct.pack_into("<h", blob, 32 + 16 * len(labels) + 2 * index, bound)
    return bytes(blob)


def combo_box(title: str, labels: list[str], start: int) -> bytes:
    blob = bytearray(32 + 16 * len(labels))
    struct.pack_into("<H", blob, 0, 3)
    blob[2:16] = title_field(title)
    struct.pack_into("<h", blob, 22, len(labels))
    struct.pack_into("<h", blob, 24, start)
    for index, label in enumerate(labels):
        encoded = label.encode("latin-1")[:15]
        offset = 32 + 16 * index
        blob[offset : offset + len(encoded)] = encoded
    return bytes(blob)


def themed_palette() -> bytes:
    def color(theme: int, value: int) -> tuple[int, int, int]:
        if theme == 0:
            rgb = (40 + value * 5 // 8, 24 + value * 3 // 4, 12 + value // 5)
        elif theme == 1:
            rgb = (70 + value * 3 // 4, 110 + value * 9 // 16,
                   150 + value * 2 // 5)
        elif theme == 2:
            rgb = (90 + value * 13 // 20, value * value // 255, value // 8)
        else:
            rgb = (value // 8, 35 + value * 3 // 5,
                   90 + value * 13 // 20)
        return tuple(min(channel, 255) for channel in rgb)

    colors = [(0, 0, 0)]
    for theme in range(4):
        count = 63 if theme == 0 else 64
        colors.extend(color(theme, step * 255 // (count - 1)) for step in range(count))

    entries = bytearray()
    for red, green, blue in colors:
        entries.extend((red, green, blue, 0))
    return struct.pack("<HH", 0x300, 256) + entries


def quoted(path: Path) -> str:
    return path.resolve().as_posix()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    delay = args.output / "mandelbrot-delay.bin"
    colors = args.output / "mandelbrot-colors.bin"
    palette = args.output / "mandelbrot-palette.bin"
    marker = args.output / "rewrite-marker.bin"
    delay.write_bytes(
        string_slider(
            "Delay:",
            ["0 sec.", "5 sec.", "15 sec.", "30 sec.", "1 min."],
            [20, 40, 60, 80, 100],
            30,
        )
    )
    colors.write_bytes(
        combo_box("Colors:", ["Earth", "Air", "Fire", "Water", "Random"], 3)
    )
    palette.write_bytes(themed_palette())
    marker.write_bytes(b"After Dark Studio clean-room rewrite\0")

    rc = args.output / "mandelbrot32.rc"
    rc.write_text(
        f'''#include <windows.h>

1 1000 "{quoted(delay)}"
2 1000 "{quoted(colors)}"
1 PAL "{quoted(palette)}"
1 AD_REWRITE "{quoted(marker)}"

1 VERSIONINFO
FILEVERSION 0,1,0,0
PRODUCTVERSION 0,1,0,0
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_DLL
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904E4"
    BEGIN
      VALUE "CompanyName", "After Dark Studio contributors\\0"
      VALUE "FileDescription", "Mandelbrot\\0"
      VALUE "FileVersion", "0.1.0\\0"
      VALUE "LegalCopyright", "Independent clean-room rewrite\\0"
      VALUE "ProductName", "After Dark Studio Classic rewrite\\0"
      VALUE "ProductVersion", "0.1.0\\0"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 1252
  END
END
''',
        encoding="ascii",
        newline="\n",
    )


if __name__ == "__main__":
    main()
