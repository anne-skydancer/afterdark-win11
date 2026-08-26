#!/usr/bin/env python3
"""
ad_inventory.py -- inventory an After Dark installation and report which
modules a native Windows 11 host can load.

The decisive fact for every .AD module is its executable format:

  * PE32 (i386) -> a real Win32 DLL. A 32-bit host process can load it on
                   Windows 11 x64 (and on ARM64, under x86 emulation).
  * NE          -> a 16-bit Windows 3.x DLL. No 64-bit Windows can load this
                   in any process. Needs OTVDM/winevdm or a reimplementation.

Also reports which engine DLL each module binds to, whether it exports the
`Module` entry point, and what it carries in resources.

Nothing here executes module code. Pure stdlib.

Usage:
    python ad_inventory.py "C:\\Program Files (x86)\\After Dark"
    python ad_inventory.py <dir> --json out.json
"""

from __future__ import annotations

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from adlib import SYSTEM_DLLS, Unsupported, open_image  # noqa: E402

SCAN_EXTS = {".ad", ".scr", ".dll", ".adm"}


def analyse(path: str) -> dict:
    rec: dict = {
        "file": os.path.basename(path),
        "path": path,
        "size": os.path.getsize(path),
        "format": "unknown",
        "has_module_export": False,
        "engine_dlls": {},
        "resources": [],
        "version": {},
        "hostable": False,
        "verdict": "",
    }

    try:
        img = open_image(path)
    except Unsupported as exc:
        rec["verdict"] = f"Not a loadable Windows image ({exc})."
        return rec

    exports = img.exports()
    rec["format"] = img.arch
    rec["is_dll"] = img.is_dll
    rec["export_count"] = len(exports)
    rec["has_module_export"] = any(e == "Module" or e.upper() == "MODULE"
                                   for e in exports)
    rec["engine_dlls"] = {k: v for k, v in img.imports().items()
                          if k.lower() not in SYSTEM_DLLS}
    rec["resources"] = img.resource_types()
    rec["version"] = img.version_strings()

    if img.kind == "NE":
        rec["ne_module_name"] = img.module_name()
        rec["ne_description"] = img.description()
        rec["hostable"] = False
        rec["verdict"] = ("16-bit NE module -- Windows 11 cannot load this in "
                          "any process. Needs OTVDM/winevdm or a rewrite.")
    elif img.hostable:
        rec["hostable"] = bool(rec["has_module_export"])
        rec["verdict"] = ("32-bit After Dark module -- loadable by a 32-bit "
                          "host on Windows 11 x64."
                          if rec["has_module_export"] else
                          "32-bit PE (support library or host executable).")
    else:
        rec["verdict"] = f"{img.arch} -- not an After Dark module."
    return rec


def scan(root: str) -> list[dict]:
    out = []
    for dirpath, _dirs, files in os.walk(root):
        for fn in sorted(files):
            if os.path.splitext(fn)[1].lower() not in SCAN_EXTS:
                continue
            full = os.path.join(dirpath, fn)
            try:
                out.append(analyse(full))
            except Exception as exc:  # keep going on odd files
                out.append({"file": fn, "path": full, "format": "error",
                            "verdict": f"could not parse: {exc}",
                            "hostable": False, "engine_dlls": {},
                            "resources": [], "version": {}})
    return out


def report(records: list[dict]) -> None:
    mods = [r for r in records if r.get("has_module_export")]
    others = [r for r in records if not r.get("has_module_export")]

    print(f"\nScanned {len(records)} file(s): "
          f"{len(mods)} After Dark module(s), {len(others)} other binaries.\n")

    if mods:
        w = max(len(r["file"]) for r in mods) + 2
        print(f"{'MODULE'.ljust(w)}{'FORMAT'.ljust(22)}{'ENGINE':<16}HOST?")
        print("-" * (w + 46))
        for r in sorted(mods, key=lambda x: (not x["hostable"], x["file"])):
            eng = ", ".join(r["engine_dlls"]) or "-"
            if len(eng) > 14:
                eng = eng[:13] + "\u2026"
            print(f"{r['file'].ljust(w)}{r['format'].ljust(22)}{eng:<16}"
                  f"{'yes' if r['hostable'] else 'NO (16-bit)'}")

    if others:
        print("\nSupport binaries / engine:")
        for r in sorted(others, key=lambda x: x["file"]):
            note = r.get("version", {}).get("FileDescription", "") or r["verdict"]
            print(f"  {r['file']:<18} {r['format']:<22} {note[:60]}")

    n_ok = sum(1 for r in mods if r["hostable"])
    n_16 = len(mods) - n_ok
    print("\n" + "=" * 62)
    print(f"  Natively hostable (32-bit PE) : {n_ok}")
    print(f"  16-bit, not natively hostable : {n_16}")
    print("=" * 62)
    if n_ok:
        engines = sorted({e for r in mods if r["hostable"]
                          for e in r["engine_dlls"]})
        if engines:
            print(f"\n  Required engine runtime: {', '.join(engines)}")
            print("  A custom host must load this from your own installation.")
    if n_16:
        print("\n  The 16-bit modules need OTVDM/winevdm, a VM, or a rewrite.")
    print()


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path", help="After Dark install directory (or a single file)")
    ap.add_argument("--json", metavar="FILE", help="also write full results as JSON")
    args = ap.parse_args()

    if not os.path.exists(args.path):
        print(f"error: no such path: {args.path}", file=sys.stderr)
        return 2

    records = [analyse(args.path)] if os.path.isfile(args.path) else scan(args.path)
    if not records:
        print("No .AD / .SCR / .DLL files found under that path.", file=sys.stderr)
        return 1

    report(records)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(records, fh, indent=2)
        print(f"Full results written to {args.json}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
