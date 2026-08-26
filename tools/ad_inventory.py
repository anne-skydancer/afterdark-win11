#!/usr/bin/env python3
"""
ad_inventory.py -- Inventory an After Dark installation and report, per module,
whether it can be hosted by a native 64-bit-era Windows 11 front-end.

The decisive fact for every .AD module is its executable format:

  * PE32 (i386)  -> a real Win32 DLL. Loadable by a 32-bit host process on
                    Windows 11 x64 via WOW64. Hostable.
  * NE           -> a 16-bit Windows 3.x DLL. Windows 11 (x64, and ARM64)
                    cannot load these at all. Not natively hostable.

It also records which engine DLL each module binds to (After Dark 4 modules
import 150-300 functions from ADXPL510.DLL), whether the module exports the
`Module` entry point, and what resources it carries -- enough for a modern
front-end to enumerate and describe modules without executing any of them.

Pure stdlib, no dependencies. Runs on Windows, macOS or Linux.

Usage:
    python ad_inventory.py "C:\\Program Files (x86)\\After Dark"
    python ad_inventory.py <dir> --json out.json
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys

SCAN_EXTS = {".ad", ".scr", ".dll", ".adm"}

# DLLs that are part of Windows, not part of After Dark.
SYSTEM_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "winmm.dll", "ole32.dll", "oleaut32.dll", "comctl32.dll", "comdlg32.dll",
    "version.dll", "winspool.drv", "msvcrt.dll", "ddraw.dll", "dsound.dll",
    "rpcrt4.dll", "shlwapi.dll", "mpr.dll", "wsock32.dll",
}

STD_RES_TYPES = {
    1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING",
    7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 11: "MESSAGETABLE",
    12: "GROUP_CURSOR", 14: "GROUP_ICON", 16: "VERSION", 24: "MANIFEST",
}


# ---------------------------------------------------------------- helpers

def _cstr(buf: bytes, off: int) -> str:
    end = buf.index(b"\0", off)
    return buf[off:end].decode("latin-1")


class Unsupported(Exception):
    pass


# ---------------------------------------------------------------- PE

class PE:
    """Minimal PE reader: exports, imports, resource types, version strings."""

    def __init__(self, data: bytes):
        self.d = data
        if data[:2] != b"MZ":
            raise Unsupported("not an MZ image")
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise Unsupported("no PE signature")
        self.machine, nsec = struct.unpack_from("<HH", data, pe + 4)
        optsz, self.characteristics = struct.unpack_from("<HH", data, pe + 20)
        opt = pe + 24
        magic = struct.unpack_from("<H", data, opt)[0]
        self.plus = magic == 0x20B
        self.dd = opt + (112 if self.plus else 96)
        self.sections = []
        for i in range(nsec):
            b = opt + optsz + i * 40
            vsz, vaddr, rsz, raddr = struct.unpack_from("<IIII", data, b + 8)
            self.sections.append((vaddr, vsz, raddr, rsz))

    @property
    def is_dll(self) -> bool:
        return bool(self.characteristics & 0x2000)

    @property
    def arch(self) -> str:
        return {0x14C: "PE32 (i386)", 0x8664: "PE32+ (x64)",
                0x1C0: "PE32 (ARM)", 0xAA64: "PE32+ (ARM64)"}.get(
            self.machine, f"PE (machine 0x{self.machine:04x})")

    def off(self, rva: int):
        for vaddr, vsz, raddr, rsz in self.sections:
            if vaddr <= rva < vaddr + max(vsz, rsz):
                delta = rva - vaddr
                if delta < rsz:
                    return raddr + delta
        return None

    def _dir(self, index: int):
        rva, size = struct.unpack_from("<II", self.d, self.dd + index * 8)
        return rva, size

    def exports(self) -> list[str]:
        rva, _ = self._dir(0)
        if not rva:
            return []
        base = self.off(rva)
        if base is None:
            return []
        n_names = struct.unpack_from("<I", self.d, base + 24)[0]
        names_rva = struct.unpack_from("<I", self.d, base + 32)[0]
        table = self.off(names_rva)
        if table is None:
            return []
        out = []
        for i in range(n_names):
            nr = struct.unpack_from("<I", self.d, table + 4 * i)[0]
            o = self.off(nr)
            if o is not None:
                out.append(_cstr(self.d, o))
        return out

    def imports(self) -> dict[str, int]:
        rva, _ = self._dir(1)
        if not rva:
            return {}
        io = self.off(rva)
        if io is None:
            return {}
        out: dict[str, int] = {}
        while True:
            olt, _ts, _fc, name_rva, fta = struct.unpack_from("<IIIII", self.d, io)
            if name_rva == 0:
                break
            no = self.off(name_rva)
            if no is None:
                break
            dll = _cstr(self.d, no)
            count = 0
            t = self.off(olt or fta)
            if t is not None:
                step = 8 if self.plus else 4
                fmt = "<Q" if self.plus else "<I"
                while True:
                    v = struct.unpack_from(fmt, self.d, t)[0]
                    if v == 0:
                        break
                    count += 1
                    t += step
            out[dll] = count
            io += 20
        return out

    def resource_types(self) -> list[str]:
        rva, _ = self._dir(2)
        if not rva:
            return []
        base = self.off(rva)
        if base is None:
            return []
        n_named, n_id = struct.unpack_from("<HH", self.d, base + 12)
        out = []
        for i in range(n_named + n_id):
            nm, _off = struct.unpack_from("<II", self.d, base + 16 + 8 * i)
            if nm & 0x80000000:
                o = base + (nm & 0x7FFFFFFF)
                ln = struct.unpack_from("<H", self.d, o)[0]
                out.append(self.d[o + 2:o + 2 + ln * 2].decode("utf-16-le", "replace"))
            else:
                out.append(STD_RES_TYPES.get(nm, f"#{nm}"))
        return out

    def _find_resource_data(self, want_type: int):
        """Walk type/name/lang and return (offset, size) of the first match."""
        rva, _ = self._dir(2)
        if not rva:
            return None
        root = self.off(rva)
        if root is None:
            return None

        def entries(dir_off):
            n_named, n_id = struct.unpack_from("<HH", self.d, dir_off + 12)
            for i in range(n_named + n_id):
                yield struct.unpack_from("<II", self.d, dir_off + 16 + 8 * i)

        for nm, off in entries(root):
            if nm & 0x80000000 or nm != want_type:
                continue
            if not (off & 0x80000000):
                continue
            lvl2 = root + (off & 0x7FFFFFFF)
            for _nm2, off2 in entries(lvl2):
                if not (off2 & 0x80000000):
                    continue
                lvl3 = root + (off2 & 0x7FFFFFFF)
                for _nm3, off3 in entries(lvl3):
                    if off3 & 0x80000000:
                        continue
                    data_rva, size = struct.unpack_from("<II", self.d, root + off3)
                    o = self.off(data_rva)
                    if o is not None:
                        return o, size
        return None

    def version_strings(self) -> dict[str, str]:
        found = self._find_resource_data(16)
        if not found:
            return {}
        off, size = found
        blob = self.d[off:off + size]
        out: dict[str, str] = {}
        # Walk the VS_VERSIONINFO tree looking for the StringFileInfo children.
        try:
            i = blob.find("StringFileInfo".encode("utf-16-le"))
            if i < 0:
                return {}
            p = i + len("StringFileInfo") * 2
            p = (p + 3) & ~3
            end = len(blob)
            # Inside: one or more StringTable blocks, each a list of String entries.
            while p + 6 <= end:
                length, vlen, vtype = struct.unpack_from("<HHH", blob, p)
                if length == 0:
                    break
                q = p + 6
                key_end = blob.find(b"\0\0", q)
                if key_end < 0:
                    break
                if (key_end - q) % 2:
                    key_end += 1
                key = blob[q:key_end].decode("utf-16-le", "replace")
                r = (key_end + 2 + 3) & ~3
                if vtype == 1 and vlen:
                    val = blob[r:r + vlen * 2].decode("utf-16-le", "replace").rstrip("\0")
                    if key and val:
                        out[key] = val
                    p += (length + 3) & ~3
                else:
                    # container (StringTable) -- descend
                    p = r
                    continue
        except Exception:
            pass
        return out


# ---------------------------------------------------------------- NE (16-bit)

class NE:
    """Minimal NE reader: module name, exported names, imported module names."""

    def __init__(self, data: bytes):
        self.d = data
        if data[:2] != b"MZ":
            raise Unsupported("not an MZ image")
        self.h = struct.unpack_from("<I", data, 0x3C)[0]
        if data[self.h:self.h + 2] != b"NE":
            raise Unsupported("no NE signature")
        self.flags = struct.unpack_from("<H", data, self.h + 0x0C)[0]
        self.target = data[self.h + 0x36] if self.h + 0x36 < len(data) else 0

    @property
    def is_dll(self) -> bool:
        return bool(self.flags & 0x8000)

    @property
    def arch(self) -> str:
        return "NE (16-bit Windows)"

    def _names_at(self, off: int) -> list[tuple[str, int]]:
        out = []
        d, p = self.d, off
        while p < len(d):
            ln = d[p]
            if ln == 0:
                break
            name = d[p + 1:p + 1 + ln].decode("latin-1", "replace")
            ordinal = struct.unpack_from("<H", d, p + 1 + ln)[0]
            out.append((name, ordinal))
            p += 1 + ln + 2
        return out

    def module_name(self) -> str:
        res = struct.unpack_from("<H", self.d, self.h + 0x26)[0]
        names = self._names_at(self.h + res)
        return names[0][0] if names else ""

    def exports(self) -> list[str]:
        names = []
        res = struct.unpack_from("<H", self.d, self.h + 0x26)[0]
        names += [n for n, o in self._names_at(self.h + res)[1:]]
        nonres = struct.unpack_from("<I", self.d, self.h + 0x2C)[0]
        if 0 < nonres < len(self.d):
            # first non-resident entry is the module DESCRIPTION
            names += [n for n, o in self._names_at(nonres)[1:]]
        return names

    def description(self) -> str:
        nonres = struct.unpack_from("<I", self.d, self.h + 0x2C)[0]
        if 0 < nonres < len(self.d):
            got = self._names_at(nonres)
            if got:
                return got[0][0]
        return ""

    def imports(self) -> dict[str, int]:
        cmod = struct.unpack_from("<H", self.d, self.h + 0x1E)[0]
        modtab = struct.unpack_from("<H", self.d, self.h + 0x28)[0]
        imptab = struct.unpack_from("<H", self.d, self.h + 0x2A)[0]
        out: dict[str, int] = {}
        for i in range(cmod):
            try:
                rel = struct.unpack_from("<H", self.d, self.h + modtab + 2 * i)[0]
                p = self.h + imptab + rel
                ln = self.d[p]
                out[self.d[p + 1:p + 1 + ln].decode("latin-1", "replace")] = 0
            except Exception:
                break
        return out


# ---------------------------------------------------------------- analysis

def analyse(path: str) -> dict:
    rec: dict = {
        "file": os.path.basename(path),
        "path": path,
        "size": os.path.getsize(path),
        "format": "unknown",
        "is_dll": None,
        "has_module_export": False,
        "engine_dlls": {},
        "resources": [],
        "version": {},
        "hostable": False,
        "verdict": "",
    }
    with open(path, "rb") as fh:
        data = fh.read()

    if data[:2] != b"MZ":
        rec["verdict"] = "Not a Windows executable (data file or resource pack)."
        return rec

    try:
        img = PE(data)
    except Unsupported:
        img = None

    if img is not None:
        rec["format"] = img.arch
        rec["is_dll"] = img.is_dll
        exps = img.exports()
        rec["has_module_export"] = "Module" in exps
        rec["export_count"] = len(exps)
        rec["engine_dlls"] = {
            k: v for k, v in img.imports().items()
            if k.lower() not in SYSTEM_DLLS
        }
        rec["resources"] = img.resource_types()
        rec["version"] = img.version_strings()
        if img.machine == 0x14C:
            rec["hostable"] = True
            if rec["has_module_export"]:
                rec["verdict"] = ("32-bit After Dark module -- loadable by a 32-bit "
                                  "host on Windows 11 x64.")
            else:
                rec["verdict"] = "32-bit PE (support library or host executable)."
        else:
            rec["verdict"] = f"{img.arch} -- not an After Dark 4 module."
        return rec

    try:
        ne = NE(data)
    except Unsupported:
        rec["verdict"] = "MZ image, but neither PE nor NE (likely DOS-only)."
        return rec

    rec["format"] = ne.arch
    rec["is_dll"] = ne.is_dll
    exps = ne.exports()
    rec["has_module_export"] = any(e.upper() == "MODULE" for e in exps)
    rec["export_count"] = len(exps)
    rec["ne_module_name"] = ne.module_name()
    rec["ne_description"] = ne.description()
    rec["engine_dlls"] = {
        k: v for k, v in ne.imports().items() if k.lower() not in SYSTEM_DLLS
    }
    rec["hostable"] = False
    rec["verdict"] = ("16-bit NE module -- Windows 11 cannot load this in any "
                      "process. Needs OTVDM/winevdm or a reimplementation.")
    return rec


def scan(root: str) -> list[dict]:
    out = []
    for dirpath, _dirs, files in os.walk(root):
        for fn in sorted(files):
            if os.path.splitext(fn)[1].lower() in SCAN_EXTS:
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
            mark = "yes" if r["hostable"] else "NO (16-bit)"
            print(f"{r['file'].ljust(w)}{r['format'].ljust(22)}{eng:<16}{mark}")

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
        engines = sorted({e for r in mods if r["hostable"] for e in r["engine_dlls"]})
        if engines:
            print(f"\n  Required engine runtime: {', '.join(engines)}")
            print("  A custom host must load this from your own installation.")
    if n_16:
        print("\n  The 16-bit modules need OTVDM/winevdm, a VM, or a rewrite.")
    print()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
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
