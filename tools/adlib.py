"""
adlib.py -- read-only reader for After Dark module binaries.

Parses both module generations without executing anything:

  * PE32 modules (After Dark 4 "AD40")  -- Win32 DLLs renamed to .AD
  * NE modules   (After Dark 2/3, and the AD4 "Classic" set) -- Win16 DLLs

and decodes the After Dark control-definition resources, which describe the
four configurable settings each module exposes. Structure layout is per
CONTROLS.TXT in the Berkeley Systems After Dark 3.0 SDK; the same 32-byte
header format is used verbatim by 32-bit AD4 modules, stored as resource
type #1000, names 1..4.

Pure stdlib. Nothing here runs module code.
"""

from __future__ import annotations

import struct

__all__ = [
    "Unsupported", "PEImage", "NEImage", "open_image", "is_module_entry",
    "decode_control", "display_name", "credits", "is_system_dll",
    "CONTROL_TYPES", "SYSTEM_DLLS", "STD_RES_TYPES",
]

SYSTEM_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "winmm.dll", "ole32.dll", "oleaut32.dll", "comctl32.dll", "comdlg32.dll",
    "version.dll", "winspool.drv", "msvcrt.dll", "ddraw.dll", "dsound.dll",
    "rpcrt4.dll", "shlwapi.dll", "mpr.dll", "wsock32.dll",
    "kernel", "user", "gdi", "keyboard", "shell", "mmsystem", "sound",
}


def is_system_dll(name: str) -> bool:
    lower = name.lower()
    return (lower in SYSTEM_DLLS or lower.startswith("api-ms-win-") or
            lower.startswith("ext-ms-"))

STD_RES_TYPES = {
    1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING",
    7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 11: "MESSAGETABLE",
    12: "GROUP_CURSOR", 14: "GROUP_ICON", 16: "VERSION", 24: "MANIFEST",
}

CONTROL_TYPES = {
    0: "none", 1: "strslider", 2: "numslider",
    3: "combobox", 4: "button", 5: "checkbox",
}

# Both generations store the four control definitions identically: resource
# type 1000, names 1..4. (The SDK ships them as loose CTRL1..CTRL4.RSC files,
# but the .RC script compiles them into type 1000.) A few third-party modules
# use the named form instead, so both are tried.
CONTROL_TYPE = "#1000"
CONTROL_NAMES = ("#1", "#2", "#3", "#4")
CONTROL_TYPES_FALLBACK = ("CTRL1", "CTRL2", "CTRL3", "CTRL4")


class Unsupported(Exception):
    pass


# A module's entry point is exported either undecorated (the Borland-built
# modules that shipped) or as the MSVC __stdcall decoration. AFTERDAR.SCR tries
# the decorated name first and falls back, and STARRYNI.AD -- the self-contained
# default that ships in ENGINE/ -- is the one that needs it.
ENTRY_NAMES = ("Module", "_Module@4")


def is_module_entry(name: str) -> bool:
    return name.upper() in ("MODULE", "_MODULE@4")


def _cstr(buf: bytes, off: int = 0) -> str:
    end = buf.find(b"\0", off)
    if end < 0:
        end = len(buf)
    return buf[off:end].decode("latin-1", "replace")


# --------------------------------------------------------------- controls

def decode_control(blob: bytes) -> dict:
    """Decode one After Dark control-definition resource.

    Layout is byte-packed with no padding (CONTROLS.TXT is explicit about
    this). Every control has a 32-byte header; sliders and combo boxes are
    followed by a 16-bytes-per-entry string array, and string sliders by a
    further 2-bytes-per-entry bounds array.
    """
    if len(blob) < 32:
        return {"type": "invalid", "title": "", "raw_size": len(blob)}

    ctl_type = struct.unpack_from("<H", blob, 0)[0]
    out: dict = {
        "type": CONTROL_TYPES.get(ctl_type, f"unknown({ctl_type})"),
        # szTitle is a fixed 14-byte field, so long labels arrive truncated.
        "title": _cstr(blob[2:16]),
    }

    def strings(count: int, base: int = 32) -> list[str]:
        return [_cstr(blob[base + 16 * i: base + 16 * i + 16])
                for i in range(count)
                if base + 16 * i + 16 <= len(blob)]

    if ctl_type == 1:  # CTL_STRSLIDER
        count, start = struct.unpack_from("<hh", blob, 22)
        count = max(0, min(count, 100))
        labels = strings(count)
        bounds_at = 32 + 16 * count
        bounds = []
        for i in range(len(labels)):
            if bounds_at + 2 * i + 2 <= len(blob):
                bounds.append(struct.unpack_from("<h", blob, bounds_at + 2 * i)[0])
        out["start_pos"] = start
        # The engine returns the *previous* bound, so the first option is 0.
        out["options"] = [
            {"label": lab, "value": 0 if i == 0 else bounds[i - 1]}
            for i, lab in enumerate(labels)
            if i - 1 < len(bounds)
        ]

    elif ctl_type == 2:  # CTL_NUMSLIDER
        out["start_pos"] = struct.unpack_from("<h", blob, 24)[0]
        out["affix"] = _cstr(blob[32:38])
        if len(blob) >= 56:
            lo, hi, intervals, mode = struct.unpack_from("<hhhh", blob, 48)
            out.update(lower=lo, upper=hi, intervals=intervals,
                       affix_mode=("none", "prefix", "suffix")[mode]
                       if 0 <= mode < 3 else mode)

    elif ctl_type == 3:  # CTL_COMBOBOX
        count, start = struct.unpack_from("<hh", blob, 22)
        count = max(0, min(count, 100))
        out["start_index"] = start
        out["options"] = strings(count)

    elif ctl_type == 5:  # CTL_CHECKBOX
        out["checked"] = bool(struct.unpack_from("<h", blob, 24)[0])

    return out


def _read_controls(img) -> list[dict]:
    """Read the four control definitions from a PE or NE module image.

    Resource lengths in NE images are rounded up to the file alignment, so a
    122-byte control can arrive as 128 bytes; decode_control tolerates the
    trailing padding.
    """
    out = []
    for i, name in enumerate(CONTROL_NAMES):
        blob = img.resource_data(CONTROL_TYPE, name)
        if blob is None:
            blob = img.resource_data(CONTROL_TYPES_FALLBACK[i], None) \
                if img.kind == "NE" else None
        out.append(decode_control(blob) if blob else
                   {"type": "none", "title": ""})
    return out


# --------------------------------------------------------------------- PE

class PEImage:
    """Minimal PE reader: exports, imports, resources, version strings."""

    kind = "PE"

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
        self.plus = struct.unpack_from("<H", data, opt)[0] == 0x20B
        self._dd = opt + (112 if self.plus else 96)
        self.sections = []
        for i in range(nsec):
            b = opt + optsz + i * 40
            vsz, vaddr, rsz, raddr = struct.unpack_from("<IIII", data, b + 8)
            self.sections.append((vaddr, vsz, raddr, rsz))
        self._res_root = None

    @property
    def is_dll(self) -> bool:
        return bool(self.characteristics & 0x2000)

    @property
    def arch(self) -> str:
        return {0x14C: "PE32 (i386)", 0x8664: "PE32+ (x64)",
                0x1C0: "PE32 (ARM)", 0xAA64: "PE32+ (ARM64)"}.get(
            self.machine, f"PE (machine 0x{self.machine:04x})")

    @property
    def hostable(self) -> bool:
        """True if a 32-bit host process on Windows 11 x64 can load this."""
        return self.machine == 0x14C

    def _off(self, rva: int):
        for vaddr, vsz, raddr, rsz in self.sections:
            if vaddr <= rva < vaddr + max(vsz, rsz):
                delta = rva - vaddr
                if delta < rsz:
                    return raddr + delta
        return None

    def _entry(self, index: int):
        return struct.unpack_from("<II", self.d, self._dd + index * 8)

    def exports(self) -> list[str]:
        rva, _ = self._entry(0)
        base = self._off(rva) if rva else None
        if base is None:
            return []
        n_names = struct.unpack_from("<I", self.d, base + 24)[0]
        table = self._off(struct.unpack_from("<I", self.d, base + 32)[0])
        if table is None:
            return []
        out = []
        for i in range(n_names):
            o = self._off(struct.unpack_from("<I", self.d, table + 4 * i)[0])
            if o is not None:
                out.append(_cstr(self.d, o))
        return out

    def imports(self) -> dict[str, int]:
        rva, _ = self._entry(1)
        io = self._off(rva) if rva else None
        if io is None:
            return {}
        out: dict[str, int] = {}
        step, fmt = (8, "<Q") if self.plus else (4, "<I")
        while True:
            olt, _ts, _fc, name_rva, fta = struct.unpack_from("<IIIII", self.d, io)
            if name_rva == 0:
                break
            no = self._off(name_rva)
            if no is None:
                break
            count = 0
            t = self._off(olt or fta)
            if t is not None:
                while struct.unpack_from(fmt, self.d, t)[0]:
                    count += 1
                    t += step
            out[_cstr(self.d, no)] = count
            io += 20
        return out

    # -- resources --

    def _root(self):
        if self._res_root is None:
            rva, _ = self._entry(2)
            self._res_root = self._off(rva) if rva else -1
        return None if self._res_root == -1 else self._res_root

    def _dir_entries(self, off: int):
        n_named, n_id = struct.unpack_from("<HH", self.d, off + 12)
        for i in range(n_named + n_id):
            yield struct.unpack_from("<II", self.d, off + 16 + 8 * i)

    def _name(self, value: int) -> str:
        root = self._root()
        if value & 0x80000000:
            o = root + (value & 0x7FFFFFFF)
            ln = struct.unpack_from("<H", self.d, o)[0]
            return self.d[o + 2:o + 2 + ln * 2].decode("utf-16-le", "replace")
        return f"#{value}"

    def resources(self) -> dict[str, list[tuple[str, int]]]:
        """{type_name: [(resource_name, size), ...]}"""
        root = self._root()
        if root is None:
            return {}
        out: dict[str, list[tuple[str, int]]] = {}
        for tval, toff in self._dir_entries(root):
            if not (toff & 0x80000000):
                continue
            tname = self._name(tval)
            items: list[tuple[str, int]] = []
            for nval, noff in self._dir_entries(root + (toff & 0x7FFFFFFF)):
                if not (noff & 0x80000000):
                    continue
                for _lval, loff in self._dir_entries(root + (noff & 0x7FFFFFFF)):
                    if loff & 0x80000000:
                        continue
                    _rva, size = struct.unpack_from("<II", self.d, root + loff)
                    items.append((self._name(nval), size))
            out[tname] = items
        return out

    def resource_types(self) -> list[str]:
        return list(self.resources())

    def resource_data(self, type_name: str, res_name: str) -> bytes | None:
        root = self._root()
        if root is None:
            return None
        for tval, toff in self._dir_entries(root):
            if self._name(tval) != type_name or not (toff & 0x80000000):
                continue
            for nval, noff in self._dir_entries(root + (toff & 0x7FFFFFFF)):
                if self._name(nval) != res_name or not (noff & 0x80000000):
                    continue
                for _lval, loff in self._dir_entries(root + (noff & 0x7FFFFFFF)):
                    if loff & 0x80000000:
                        continue
                    rva, size = struct.unpack_from("<II", self.d, root + loff)
                    o = self._off(rva)
                    if o is not None:
                        return self.d[o:o + size]
        return None

    def controls(self) -> list[dict]:
        return _read_controls(self)

    def version_strings(self) -> dict[str, str]:
        blob = self.resource_data("#16", "#1")
        if not blob:
            return {}
        out: dict[str, str] = {}
        anchor = blob.find("StringFileInfo".encode("utf-16-le"))
        if anchor < 0:
            return {}
        p = (anchor + len("StringFileInfo") * 2 + 3) & ~3
        try:
            while p + 6 <= len(blob):
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
                    val = blob[r:r + vlen * 2].decode("utf-16-le", "replace")
                    val = val.rstrip("\0")
                    if key and val:
                        out[key] = val
                    p += (length + 3) & ~3
                else:
                    p = r  # descend into StringTable
        except Exception:
            pass
        return out


# --------------------------------------------------------------------- NE

class NEImage:
    """Minimal NE (16-bit) reader: names, imports and the resource table."""

    kind = "NE"
    machine = 0
    arch = "NE (16-bit Windows)"
    hostable = False

    def __init__(self, data: bytes):
        self.d = data
        if data[:2] != b"MZ":
            raise Unsupported("not an MZ image")
        self.h = struct.unpack_from("<I", data, 0x3C)[0]
        if data[self.h:self.h + 2] != b"NE":
            raise Unsupported("no NE signature")
        self.flags = struct.unpack_from("<H", data, self.h + 0x0C)[0]

    @property
    def is_dll(self) -> bool:
        return bool(self.flags & 0x8000)

    def _names_at(self, off: int) -> list[tuple[str, int]]:
        out, d, p = [], self.d, off
        while 0 <= p < len(d):
            ln = d[p]
            if ln == 0 or p + 1 + ln + 2 > len(d):
                break
            out.append((d[p + 1:p + 1 + ln].decode("latin-1", "replace"),
                        struct.unpack_from("<H", d, p + 1 + ln)[0]))
            p += 1 + ln + 2
        return out

    def module_name(self) -> str:
        res = struct.unpack_from("<H", self.d, self.h + 0x26)[0]
        got = self._names_at(self.h + res)
        return got[0][0] if got else ""

    def description(self) -> str:
        nonres = struct.unpack_from("<I", self.d, self.h + 0x2C)[0]
        if 0 < nonres < len(self.d):
            got = self._names_at(nonres)
            if got:
                return got[0][0]
        return ""

    def exports(self) -> list[str]:
        res = struct.unpack_from("<H", self.d, self.h + 0x26)[0]
        names = [n for n, _o in self._names_at(self.h + res)[1:]]
        nonres = struct.unpack_from("<I", self.d, self.h + 0x2C)[0]
        if 0 < nonres < len(self.d):
            names += [n for n, _o in self._names_at(nonres)[1:]]
        return names

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

    # -- resources --

    def _resources_raw(self) -> dict[str, list[tuple[str, int, int]]]:
        """{type: [(name, file_offset, length)]} from the NE resource table."""
        rsrc = struct.unpack_from("<H", self.d, self.h + 0x24)[0]
        restab = struct.unpack_from("<H", self.d, self.h + 0x26)[0]
        if not rsrc or rsrc == restab:      # no resource table present
            return {}
        base = self.h + rsrc
        shift = struct.unpack_from("<H", self.d, base)[0]
        if shift > 16:
            return {}

        def label(val: int, is_type: bool) -> str:
            # Integer ids: only resource *types* map to the standard names.
            # A resource *name* of 1 is "#1", not "CURSOR".
            if val & 0x8000:
                rid = val & 0x7FFF
                return STD_RES_TYPES.get(rid, f"#{rid}") if is_type else f"#{rid}"
            o = base + val
            if o >= len(self.d):
                return f"?{val}"
            ln = self.d[o]
            return self.d[o + 1:o + 1 + ln].decode("latin-1", "replace")

        out: dict[str, list[tuple[str, int, int]]] = {}
        p = base + 2
        while p + 8 <= len(self.d):
            type_id, count = struct.unpack_from("<HH", self.d, p)
            if type_id == 0:
                break
            p += 8
            items = []
            for _ in range(count):
                if p + 12 > len(self.d):
                    break
                off, length, _flags, rid = struct.unpack_from("<HHHH", self.d, p)
                items.append((label(rid, False), off << shift, length << shift))
                p += 12
            out.setdefault(label(type_id, True), []).extend(items)
        return out

    def resources(self) -> dict[str, list[tuple[str, int]]]:
        return {t: [(n, ln) for n, _o, ln in v]
                for t, v in self._resources_raw().items()}

    def resource_types(self) -> list[str]:
        return list(self._resources_raw())

    def resource_data(self, type_name: str, res_name: str | None = None):
        for name, off, length in self._resources_raw().get(type_name, []):
            if res_name is None or name == res_name:
                return self.d[off:off + length]
        return None

    @property
    def hostable_reason(self) -> str:
        return "16-bit NE image; no 64-bit Windows can load it"

    def controls(self) -> list[dict]:
        return _read_controls(self)

    def version_strings(self) -> dict[str, str]:
        desc = self.description()
        return {"FileDescription": desc} if desc else {}


def _stringlist(blob: bytes | None) -> list[str]:
    """Decode a STRINGLIST resource: a WORD count, then packed strings."""
    if not blob or len(blob) < 2:
        return []
    body = blob[2:]
    return [p.decode("latin-1", "replace").strip()
            for p in body.split(b"\0") if p.strip()]


def display_name(img) -> str:
    """The module's own human-readable name, if it carries one.

    AD4 modules put it in VERSION/FileDescription, with STRINGLIST 128 as a
    second source. Classic modules use the MNAME resource (type 2000, name 20);
    failing that, the NE resident module name is still better than nothing.
    """
    if img.kind == "PE":
        for key in ("FileDescription", "ProductName"):
            val = img.version_strings().get(key, "").strip()
            if val:
                return val
        got = _stringlist(img.resource_data("STRINGLIST", "#128"))
        if got:
            return got[0]
        return ""
    mname = img.resource_data("#2000", "#20")
    if mname:
        text = _cstr(mname).strip()
        if text:
            return text
    return (img.module_name() or "").strip()


def credits(img) -> str:
    """Author/copyright line, where the module carries one."""
    if img.kind == "NE":
        blob = img.resource_data("#2000", "#10")
        if blob:
            return " ".join(_cstr(blob).split())
        return img.description().strip()
    ver = img.version_strings()
    return (ver.get("LegalCopyright") or ver.get("CompanyName") or "").strip()


# ------------------------------------------------------------------ facade

def open_image(path: str):
    """Return a PEImage or NEImage for `path`, or raise Unsupported."""
    with open(path, "rb") as fh:
        data = fh.read()
    if data[:2] != b"MZ":
        raise Unsupported("not a Windows executable")
    try:
        return PEImage(data)
    except Unsupported:
        return NEImage(data)
