#!/usr/bin/env python3
"""
ad_extract.py -- extract a configuration schema for every After Dark module
in an installation, as JSON, without executing any module code.

Each After Dark module exposes up to four settings, described by control
resources baked into the module binary. This reads them and emits a schema a
modern configuration UI can render directly: sliders with their labelled
stops, combo boxes with their choices, checkboxes with their defaults.

This works for BOTH module generations -- including the 16-bit Classic
modules that Windows 11 cannot execute. Their settings are still fully
readable, which means a reimplementation can honour the original controls
exactly.

Usage:
    python ad_extract.py "C:\\Program Files (x86)\\After Dark" -o modules.json
    python ad_extract.py TOASTERS.AD            # pretty-print one module
"""

from __future__ import annotations

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from adlib import (SYSTEM_DLLS, Unsupported, credits,  # noqa: E402
                   display_name, open_image)

SCAN_EXTS = {".ad", ".adm"}


def module_title(img, path: str) -> str:
    """Best available human name for a module."""
    name = display_name(img)
    # Linker boilerplate is not a title.
    if name and "graphics module library" not in name.lower():
        return name
    return os.path.splitext(os.path.basename(path))[0].title()


def extract(path: str) -> dict:
    img = open_image(path)
    exports = img.exports()
    is_module = any(e == "Module" or e.upper() == "MODULE" for e in exports)
    engine = {k: v for k, v in img.imports().items()
              if k.lower() not in SYSTEM_DLLS}
    resources = img.resources()

    # The engine addresses controls by slot (iControlValue[0..3]), so the
    # slot index is preserved even though unused slots are dropped.
    controls = []
    if is_module:
        for i, ctl in enumerate(img.controls()):
            if ctl.get("type") == "none":
                continue
            ctl["index"] = i
            controls.append(ctl)

    rec = {
        "file": os.path.basename(path),
        "path": path,
        "title": module_title(img, path),
        "credits": credits(img),
        "format": img.arch,
        "generation": "AD4 (32-bit)" if img.kind == "PE" else "Classic (16-bit)",
        "is_module": is_module,
        "hostable": bool(img.hostable and is_module),
        "engine": list(engine),
        "controls": controls,
        "assets": {
            "sounds": len(resources.get("WAV", [])),
            "bitmaps": len(resources.get("BITMAP", [])),
            "palettes": len(resources.get("PAL", [])),
            "art_blocks": len(resources.get("#8000", []))
                          + len(resources.get("#8001", [])),
        },
        "version": img.version_strings(),
    }
    if not rec["hostable"] and is_module:
        rec["needs"] = ("reimplementation or OTVDM -- Windows 11 cannot load "
                        "16-bit modules")
    return rec


def scan(root: str) -> list[dict]:
    out = []
    for dirpath, _dirs, files in os.walk(root):
        for fn in sorted(files):
            if os.path.splitext(fn)[1].lower() not in SCAN_EXTS:
                continue
            full = os.path.join(dirpath, fn)
            try:
                rec = extract(full)
            except Exception as exc:  # never let one bad file stop the scan
                rec = {"file": fn, "path": full, "error": str(exc),
                       "is_module": False, "hostable": False, "controls": []}
            out.append(rec)
    return out


def describe(control: dict) -> str:
    kind = control.get("type")
    title = control.get("title") or "(untitled)"
    if kind == "strslider":
        opts = " / ".join(o["label"] for o in control.get("options", []))
        return f"    slider   {title:<16} [{opts}]"
    if kind == "numslider":
        return (f"    number   {title:<16} "
                f"{control.get('lower')}..{control.get('upper')} "
                f"({control.get('intervals')} steps)")
    if kind == "combobox":
        return f"    choice   {title:<16} [{' / '.join(control.get('options', []))}]"
    if kind == "checkbox":
        return f"    checkbox {title:<16} default={control.get('checked')}"
    if kind == "button":
        return f"    button   {title}"
    return f"    {kind}"


def report(records: list[dict]) -> None:
    mods = [r for r in records if r.get("is_module")]
    configurable = [r for r in mods if r.get("controls")]
    print(f"\n{len(mods)} module(s); {len(configurable)} expose settings.\n")
    for r in sorted(mods, key=lambda x: (not x["hostable"], x["file"])):
        tag = "hostable" if r["hostable"] else "16-bit"
        print(f"  {r['title']}  ({r['file']}, {tag})")
        if r.get("controls"):
            for c in r["controls"]:
                print(describe(c))
        else:
            print("    (no configurable settings)")
        print()


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path", help="After Dark install directory, or one .AD file")
    ap.add_argument("-o", "--output", metavar="FILE", help="write JSON schema")
    args = ap.parse_args()

    if not os.path.exists(args.path):
        print(f"error: no such path: {args.path}", file=sys.stderr)
        return 2

    if os.path.isfile(args.path):
        try:
            records = [extract(args.path)]
        except Unsupported as exc:
            print(f"error: {args.path}: {exc}", file=sys.stderr)
            return 2
    else:
        records = scan(args.path)
    if not records:
        print("No .AD files found under that path.", file=sys.stderr)
        return 1

    report(records)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as fh:
            json.dump(records, fh, indent=2)
        print(f"Schema written to {args.output}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
