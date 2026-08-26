# afterdark-win11

A modern UI for **After Dark 4** screensavers on Windows 11 — replacing the
copy-`.scr`-to-`System32` and import-a-registry-blob ritual with an actual
application, in which every module is properly configurable.

## Where this stands

Feasibility is settled, the configuration layer is built, and the module ABI
has been recovered by disassembly — the one unknown that gated a native host.
What remains is the Windows app itself.

| Document | What it covers |
|---|---|
| [docs/FEASIBILITY.md](docs/FEASIBILITY.md) | Verdict and evidence: module ABI, engine dependency, what Windows 11 can and cannot load |
| [docs/DESIGN.md](docs/DESIGN.md) | The application: architecture, per-module configuration, screensaver registration without registry hacks |
| [docs/ABI.md](docs/ABI.md) | The AD4 module ABI, recovered by disassembly: entry point, 348-byte parameter block, message numbering, lifecycle |
| [docs/REWRITES.md](docs/REWRITES.md) | The 61 16-bit Classic modules, and how to bring them forward as 32-bit rewrites |

**The short version.** After Dark 4's own (`AD40`) modules are 32-bit PE DLLs
exporting a `Module` entry point against the `ADXPL510.DLL` engine — Windows 11
can still load these, so a 32-bit host process can drive them directly. The
bundled **Classic** modules are 16-bit NE DLLs that no 64-bit Windows can load
in any process; they need OTVDM or a rewrite.

Crucially, **every module's settings are readable from its binary**, in both
generations — so the configuration UI needs no help from the dead Windows 95
control panel, and rewrites of the 16-bit modules can honour the original
controls exactly.

## Tools

Both are read-only, stdlib-only Python 3. They parse module binaries; they
never execute them.

### Inventory — what can actually run

```
python tools\ad_inventory.py "C:\Program Files (x86)\After Dark"
```

```
MODULE       FORMAT                ENGINE          HOST?
-----------------------------------------------------------
BADDOG.AD    PE32 (i386)           adxpl510.dll    yes
TOASTERS.AD  PE32 (i386)           adxpl510.dll    yes
BORIS.AD     NE (16-bit Windows)   AD_RSRC, AD_S…  NO (16-bit)
```

### Extract — every module's configuration schema

```
python tools\ad_extract.py "C:\Program Files (x86)\After Dark" -o modules.json
```

```
Flying Toasters!  (TOASTERS.AD, hostable)
    slider   Objects:         [Flight / Squadron / Air Wing / Swarm]
    choice   Toasters:        [Adults / Babies / Random]
    slider   Music:           [Never / 1 Min. / 2 Mins. / 5 Mins. / 30 Mins. / Always]
    checkbox Display Karaok   default=True

Boris  (BORIS.AD, 16-bit)
    number   Number of Cats   0..3
    slider   Butterfly:       [Never / Rarely / Seldom / Occasionally / Frequently / Always]
```

The JSON is the direct input to the configuration UI: control type, labels,
defaults, and the slot index each value belongs in.

## Building a host

[`include/ad_module32.h`](include/ad_module32.h) declares the recovered ABI:

```c
int __stdcall Module(AD_MODULE32 *params);   /* "_Module@4", else "Module" */
```

A 348-byte parameter block carries the message at `+0x154` and the four control
values at `+0x40`. Note AD4 **renumbered** the messages from After Dark 3
(`BLANK=3`, `DRAWFRAME=4`, `CLOSE=5`) — building to the published AD3 constants
will send the wrong ones. [docs/ABI.md](docs/ABI.md) has the evidence.

```
cc -o abitest tests/test_ad_module32_layout.c && ./abitest
```

verifies the header still matches the disassembled offsets.

`tools/adlib.py` is the shared reader — PE and NE images, resources, and the
After Dark control-definition format.

## A note on content

This repository contains no Berkeley Systems code or artwork, and should never
contain any. Everything here reads an installation you already own, on your own
machine, at runtime. `.gitignore` is set up to keep it that way.
