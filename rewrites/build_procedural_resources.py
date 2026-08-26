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


def checkbox(title: str, checked: bool) -> bytes:
    blob = bytearray(32)
    struct.pack_into("<H", blob, 0, 5)
    blob[2:16] = title_field(title)
    struct.pack_into("<h", blob, 24, int(checked))
    return bytes(blob)


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
    marker_path = output / "rewrite-marker.bin"
    marker_path.write_bytes(b"After Dark Studio clean-room rewrite\0")

    resources = "\n".join(
        f'{index} 1000 "{quoted(path)}"'
        for index, path in enumerate(paths, start=1)
    )
    rc = output / f"{stem}.rc"
    rc.write_text(
        f'''#include <windows.h>

{resources}
1 PAL "{quoted(palette_path)}"
1 AD_REWRITE "{quoted(marker_path)}"

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
    write_module(
        args.output,
        "warp32",
        "Warp!",
        [
            string_slider(
                "Speed:",
                ["Fast In", "Medium In", "Slow In", "Impulse In",
                 "Impulse Out", "Slow Out", "Medium Out", "Fast Out"],
                [12, 25, 38, 45, 55, 75, 88, 100],
                0,
            ),
            number_slider("Stars:", 1, 200, 1, 71),
            combo_box("Size:", ["Small", "Big", "Both"], 0),
            checkbox("Color", False),
        ],
    )
    write_module(
        args.output,
        "spheres32",
        "Spheres",
        [
            number_slider("Max Size:", 10, 100, 100, 73, "%", 2),
            number_slider("Offset:", 0, 10, 100, 27),
            number_slider("Clear Every:", 1, 200, 100, 30),
            checkbox("Clear Screen F", False),
        ],
    )
    write_module(
        args.output,
        "stained32",
        "Stained Glass",
        [
            number_slider("Complexity:", 0, 100, 1, 10, "%", 2),
            number_slider("Duplication:", 0, 100, 1, 100, "%", 2),
            number_slider("Color:", 0, 100, 1, 100, "%", 2),
            none(),
        ],
    )
    write_module(
        args.output,
        "string32",
        "String Theory",
        [
            combo_box("String Groups:", ["1", "2", "3", "4"], 2),
            string_slider(
                "Strings:",
                ["10", "20", "30", "40", "50", "60", "70", "80",
                 "90", "100", "110", "120", "130", "140", "150",
                 "Infinite"],
                [12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84,
                 90, 95, 100],
                87,
            ),
            number_slider("Color Speed:", 1, 100, 100, 96, "%", 2),
            checkbox("Clear Screen F", False),
        ],
    )
    write_module(
        args.output,
        "photon32",
        "Photon",
        [
            string_slider(
                "Length",
                ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                 "12", "14", "16", "20", "24", "28", "32"],
                [18, 26, 33, 39, 44, 49, 54, 59, 64, 69, 74, 79, 84, 89,
                 94, 100, 101],
                58,
            ),
            string_slider(
                "Burst Delay",
                ["None", "1/2 sec.", "1 sec.", "2 sec.", "5 sec.",
                 "10 sec.", "30 sec."],
                [10, 20, 40, 60, 80, 90, 100],
                0,
            ),
            checkbox("Always Centere", True),
            combo_box("Burst", ["Mixed", "Photon", "Electron", "Proton",
                                "Neutrino"], 0),
        ],
    )
    write_module(
        args.output,
        "strange32",
        "Strange Attract",
        [
            string_slider(
                "Duration:",
                ["5 seconds", "10 seconds", "15 seconds", "20 seconds",
                 "30 seconds", "45 seconds", "1 minute", "2 minutes",
                 "5 minutes", "10 minutes", "15 minutes", "20 minutes",
                 "30 minutes", "45 minutes", "1 hour"],
                [15, 20, 27, 34, 40, 47, 54, 60, 67, 74, 80, 87, 94, 100,
                 101],
                30,
            ),
            string_slider(
                "Color Speed:",
                ["None", "Slowest", "Slower", "Slow", "Normal", "Fast",
                 "Faster", "Fastest"],
                [24, 36, 48, 60, 72, 84, 94, 100],
                66,
            ),
            none(),
            none(),
        ],
    )


if __name__ == "__main__":
    main()