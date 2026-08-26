# afterdark-win11

A modern UI for **After Dark 4** screensavers on Windows 11 — replacing the
copy-`.scr`-to-`System32` and import-a-registry-blob ritual with an actual
application, in which every module is properly configurable.

## Download

**[AfterDarkStudio-0.1.6-setup.exe](releases/AfterDarkStudio-0.1.6-setup.exe)** —
64-bit, system-wide, self-contained (49.9 MiB). Checksum and full notes in
[releases/](releases/README.md).

The Studio UI and real disc catalogue have now been exercised on Windows 11.
The installer is not code-signed, so SmartScreen will warn.

It ships no screen savers. Setup imports them from your own After Dark 4 disc or
installation — which also replaces AD4's own installer, that being 16-bit and
unable to run on Windows 11 at all.

Companion media stays user-owned too. In particular, Setup imports Art Critic's
`PICTURES` folder from the disc and Studio points the original 32-bit module at
that local copy; neither the images nor a modified module are distributed.
Setup also imports the five external MIDI tracks needed by Points of View,
Slow Burn, Swirling Magic, and Flying Toasters. Embedded sound effects continue
to load from the user's original module files.

## Where this stands

Feasibility is settled, the module ABI has been recovered by disassembly, a
working host renders real After Dark 4 modules, and the app around it now
exists: a native WPF/.NET 10 catalogue and settings UI, a 64-bit `.scr` that drives the
whole screensaver path, registration without any registry hacks, and an Inno
Setup 7 installer.

| Document | What it covers |
|---|---|
| [docs/FEASIBILITY.md](docs/FEASIBILITY.md) | Verdict and evidence: module ABI, engine dependency, what Windows 11 can and cannot load |
| [docs/DESIGN.md](docs/DESIGN.md) | The application: architecture, per-module configuration, screensaver registration without registry hacks |
| [docs/PACKAGING.md](docs/PACKAGING.md) | Architecture (what is 64-bit and what cannot be), the screensaver flow, registration, and the Inno Setup 7 installer |
| [host/README.md](host/README.md) | `admhost32` — a working 32-bit host that loads and renders real modules |
| [docs/ABI.md](docs/ABI.md) | The AD4 module ABI, recovered by disassembly: entry point, 348-byte parameter block, message numbering, lifecycle |
| [docs/REWRITES.md](docs/REWRITES.md) | The 61 16-bit Classic modules, and how to bring them forward as 32-bit rewrites |
| [archive/AfterDark.Studio.Avalonia](archive/AfterDark.Studio.Avalonia) | Buildable archive of the former Avalonia Studio shell |

**The short version.** After Dark 4's own (`AD40`) modules are 32-bit PE DLLs
exporting a `Module` entry point against the `ADXPL510.DLL` engine — Windows 11
can still load these, so a 32-bit host process can drive them directly. The
bundled **Classic** modules are 16-bit NE DLLs that no 64-bit Windows can load
in any process; they need OTVDM or a rewrite.

### Coverage, checked against all 84 modules on the AD4 disc

| Set | Count | Runs on Windows 11 | Settings readable |
|---|---:|---|---|
| `AD40/` | 22 | yes | yes |
| `ENGINE/STARRYNI.AD` | 1 | yes | yes |
| `CLASSIC/` (the After Dark 3-era library) | 61 | **no — 16-bit** | yes |
| **Total** | **84** | **23** | **84** |

All 84 expose settings — 248 controls in total (229 value-carrying, plus 19
buttons that open a module's own dialog). **After Dark 3 is entirely 16-bit**:
its engine, `ADXPL300.DLL`, is itself an NE binary, so no AD3 screensaver can
run on 64-bit Windows by any means short of emulation or a rewrite.

**This repository contains none of them.** They are imported from your own disc
or installation — see below.

Crucially, **every module's settings are readable from its binary**, in both
generations — so the configuration UI needs no help from the dead Windows 95
control panel, and rewrites of the 16-bit modules can honour the original
controls exactly.

## The application

```
AfterDark.Studio.exe   x64   catalogue, per-module settings, live preview, registration
AfterDarkModern.scr    x64   /s /p /c, full-screen windows, input, multi-monitor
admhost32.exe          x86   loads ADXPL510.DLL + the module and renders
```

Everything is 64-bit except the one process that touches a module, and that is
not a choice: a 64-bit process cannot load a 32-bit DLL. HWNDs cross the
boundary fine, so the x64 `.scr` owns the windows and an x86 child renders into
them. See [docs/PACKAGING.md](docs/PACKAGING.md).

Studio remains in the Windows notification area when its window is closed. The
`AD` tray menu switches directly between runnable modules, reopens Studio, and
links to the native Windows screensaver settings. Windows remains the sole
owner of the idle timeout and sign-in-on-resume preferences.

```
make dist        # build everything into dist/
make rewrite     # build the independent MANDEL32.AD and SHAPES32.AD rewrites
make test        # ABI layout check + catalogue tests
make installer   # Inno Setup 7 (Windows)
```

Installs **system-wide** into Program Files for every user (Inno Setup 7,
`SetupArchitecture=x64`). Which screensaver is *active* stays per-user — the
screensaver policy is User Configuration and operates on `HKCU\Control
Panel\Desktop`, and no HKLM equivalent exists — so the installer seeds a
machine-wide default under `%ProgramData%` and each user activates it for
themselves. Still no `.reg` import and no hand-editing.

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
