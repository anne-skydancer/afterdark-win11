#!/usr/bin/env python3
"""Generate independent Shapes control and version resources."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def checkbox(title: str, checked: bool) -> bytes:
    blob = bytearray(32)
    struct.pack_into("<H", blob, 0, 5)
    encoded = title.encode("latin-1")[:14]
    blob[2 : 2 + len(encoded)] = encoded
    struct.pack_into("<h", blob, 24, int(checked))
    return bytes(blob)


def none() -> bytes:
    return bytes(32)


def palette() -> bytes:
  colors = [
    (0x00, 0x00, 0x00),
    (0xF2, 0xB3, 0x3D),
    (0x5B, 0xC9, 0x8B),
    (0x4D, 0xA3, 0xFF),
    (0xF0, 0x6A, 0x8A),
    (0xD9, 0x8C, 0xFF),
    (0xF4, 0xF4, 0xF6),
    (0x58, 0xD6, 0xE8),
    (0xF0, 0x9A, 0x55),
  ]
  colors.extend((value, value, value) for value in range(1, 248))
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

    clear = args.output / "shapes-clear.bin"
    color = args.output / "shapes-color.bin"
    unused3 = args.output / "shapes-unused3.bin"
    unused4 = args.output / "shapes-unused4.bin"
    colors = args.output / "shapes-palette.bin"
    marker = args.output / "rewrite-marker.bin"
    clear.write_bytes(checkbox("Clear Screen F", False))
    color.write_bytes(checkbox("Color", True))
    unused3.write_bytes(none())
    unused4.write_bytes(none())
    colors.write_bytes(palette())
    marker.write_bytes(b"After Dark Studio clean-room rewrite\0")

    rc = args.output / "shapes32.rc"
    rc.write_text(
        f'''#include <windows.h>

1 1000 "{quoted(clear)}"
2 1000 "{quoted(color)}"
3 1000 "{quoted(unused3)}"
4 1000 "{quoted(unused4)}"
1 PAL "{quoted(colors)}"
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
      VALUE "FileDescription", "Shapes\\0"
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