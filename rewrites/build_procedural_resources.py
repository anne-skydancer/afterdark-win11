#!/usr/bin/env python3
"""Generate controls, palettes, and version resources for procedural rewrites."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def title_field(title: str) -> bytes:
    encoded = title.encode("latin-1")[:14]
    return encoded + bytes(14 - len(encoded))


def none() -> bytes:
    return bytes(32)


def number_slider(
    title: str,
    lower: int,
    upper: int,
    intervals: int,
    start: int,
    affix: str = "",
    affix_mode: int = 0,
) -> bytes:
    blob = bytearray(56)
    struct.pack_into("<H", blob, 0, 2)
    blob[2:16] = title_field(title)
    struct.pack_into("<h", blob, 24, start)
    encoded = affix.encode("latin-1")[:5]
    blob[32 : 32 + len(encoded)] = encoded
    struct.pack_into("<hhhh", blob, 48, lower, upper, intervals, affix_mode)
    return bytes(blob)


def combo_box(title: str, labels: list[str], start: int) -> bytes:
    blob = bytearray(32 + 16 * len(labels))
    struct.pack_into("<H", blob, 0, 3)
    blob[2:16] = title_field(title)
    struct.pack_into("<hh", blob, 22, len(labels), start)
    for index, label in enumerate(labels):
        encoded = label.encode("latin-1")[:15]
        offset = 32 + 16 * index
        blob[offset : offset + len(encoded)] = encoded
    return bytes(blob)


def string_slider(
    title: str, labels: list[str], bounds: list[int], start: int
) -> bytes:
    blob = bytearray(32 + 16 * len(labels) + 2 * len(bounds))
    struct.pack_into("<H", blob, 0, 1)
    blob[2:16] = title_field(title)
    struct.pack_into("<hh", blob, 22, len(labels), start)
    for index, label in enumerate(labels):
        encoded = label.encode("latin-1")[:15]
        offset = 32 + 16 * index
        blob[offset : offset + len(encoded)] = encoded
    for index, bound in enumerate(bounds):
        struct.pack_into("<h", blob, 32 + 16 * len(labels) + 2 * index, bound)
    return bytes(blob)


def palette() -> bytes:
    colors = [
        (0x00, 0x00, 0x00),
        (0xF2, 0xB3, 0x3D), (0x5B, 0xC9, 0x8B), (0x4D, 0xA3, 0xFF),
        (0xF0, 0x6A, 0x8A), (0xD9, 0x8C, 0xFF), (0xF4, 0xF4, 0xF6),
        (0x58, 0xD6, 0xE8), (0xF0, 0x9A, 0x55), (0xFF, 0xF1, 0x76),
        (0x78, 0xD4, 0xA7), (0x6C, 0xB6, 0xFF), (0xFF, 0x8A, 0xA4),
        (0xE4, 0xA7, 0xFF), (0xFF, 0xFF, 0xFF), (0x7C, 0xE7, 0xF5),
        (0xFF, 0xB5, 0x70),
    ]
    colors.extend((value, value, value) for value in range(1, 240))
    entries = bytearray()
    for red, green, blue in colors:
        entries.extend((red, green, blue, 0))
    return struct.pack("<HH", 0x300, 256) + entries


def quoted(path: Path) -> str:
    return path.resolve().as_posix()


def write_module(
    output: Path, stem: str, title: str, controls: list[bytes]
) -> None:
    paths: list[Path] = []
    for index, control in enumerate(controls, start=1):
        path = output / f"{stem}-control{index}.bin"
        path.write_bytes(control)
        paths.append(path)
    palette_path = output / f"{stem}-palette.bin"
    palette_path.write_bytes(palette())

    resources = "\n".join(
        f'{index} 1000 "{quoted(path)}"'
        for index, path in enumerate(paths, start=1)
    )
    rc = output / f"{stem}.rc"
    rc.write_text(
        f'''#include <windows.h>

{resources}
1 PAL "{quoted(palette_path)}"

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
      VALUE "FileDescription", "{title}\\0"
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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    write_module(
        args.output,
        "spiral32",
        "Spiral Gyra",
        [
            number_slider("Max Lines:", 20, 360, 10, 54),
            number_slider("Min Lines:", 0, 100, 1, 28, "%", 2),
            number_slider("Color Cycling:", 0, 100, 1, 32, "%", 2),
            none(),
        ],
    )
    write_module(
        args.output,
        "tunnel32",
        "Tunnel",
        [
            combo_box("Direction:", ["In", "Out"], 0),
            combo_box("Shape:", ["Rect", "R-Rect", "Random"], 1),
            none(),
            none(),
        ],
    )
    write_module(
        args.output,
        "zot32",
        "Zot!",
        [
            string_slider("Forkiness:", ["Few", "Forky", "Max Forky!"],
                          [33, 67, 100], 50),
            none(),
            string_slider("How Often:",
                          ["Rarely", "Sometimes", "Often", "Stormy!"],
                          [25, 50, 75, 100], 62),
            none(),
        ],
    )


if __name__ == "__main__":
    main()