# After Dark Studio — design

A modern Windows 11 application that replaces the copy-to-`System32`-and-import-a-
registry-blob ritual with an actual UI: browse every module, configure each one
properly, preview it live, and set it as your screensaver.

---

## 1. What this replaces

The method currently circulating for getting After Dark 4 working on Windows 10/11 is:

1. Copy `After Dark.scr` into `C:\Windows\System32`.
2. Import a `.reg` file (typically sourced from a since-deleted video) to
   recreate registry entries the 16-bit installer can no longer write.
3. Select "After Dark" in the classic screensaver dialog.
4. Accept that module configuration is mostly unreachable, because AD4's
   control panel hooked a Windows 95 shell extension point that no longer exists.

Every step of that is avoidable:

| The old ritual | Why it existed | What replaces it |
|---|---|---|
| Copy `.scr` to `System32` | So the classic dialog would list it | Unnecessary. `HKCU\Control Panel\Desktop\SCRNSAVE.EXE` accepts a **full path**. Set it programmatically — no admin rights, no writes to a system directory. |
| Import a `.reg` blob | The 16-bit installer couldn't run | The app discovers modules by **scanning a folder you point it at**. No installer, no registry seeding, no trusting an opaque `.reg` from the internet. |
| Buried classic dialog | Windows moved the UI | The app writes the same registry values itself, and offers a modern settings surface. |
| Unreachable module settings | Dead shell extension | Controls are **read out of the module binaries** and rendered natively. This is already implemented — see §3. |

The key realisation: nothing about After Dark's module format requires
Berkeley Systems' control panel. The settings are in the binaries, the
screensaver contract is a documented three-flag command line, and the
registry keys are three values under `HKCU`. All of it is reachable.

---

## 2. Architecture

A 64-bit UI cannot load a 32-bit module DLL, so the process split is forced by
the platform — but it is also exactly what you would choose anyway, because it
contains crashes in 1996-vintage code.

```
┌──────────────────────────────────────────────────────────────┐
│  AfterDarkStudio.exe            x64, WinUI 3 / Avalonia      │
│                                                              │
│  Module gallery · per-module settings · live preview ·       │
│  "Set as screensaver" · idle timeout · multi-monitor rules   │
│                                                              │
│  Owns: settings store (JSON), module catalogue, registry     │
└───────────────┬──────────────────────────────────────────────┘
                │  IPC: named pipe + shared-memory surface
                │  (control values down, frames/state up)
┌───────────────▼──────────────────────────────────────────────┐
│  admhost32.exe                  x86 — REQUIRED               │
│                                                              │
│  LoadLibrary(ADXPL510.DLL)   <- from the user's install      │
│  LoadLibrary(FOO.AD)                                          │
│  GetProcAddress("Module")                                     │
│  Module(INITIALIZE|BLANK|DRAWFRAME|CLOSE, hdc, &params)       │
│                                                              │
│  Owns: frame pacing, offscreen DIB, palette emulation        │
└───────────────┬──────────────────────────────────────────────┘
                │
┌───────────────▼──────────────────────────────────────────────┐
│  AfterDarkModern.scr            thin launcher                │
│  /s full-screen · /p <hwnd> preview · /c config              │
└──────────────────────────────────────────────────────────────┘
```

**Why three binaries and not two.** The `.scr` must be small and start fast;
the UI shell wants to be a normal modern app; the module host must be x86. Any
merge of these compromises one of the three. The `.scr` simply spawns
`admhost32.exe` in full-screen mode, or hands `/c` to the UI shell.

On ARM64 Windows 11 the whole stack still works — `admhost32.exe` runs under
x86 emulation.

---

## 3. Per-module configuration

**This is already solved, and the implementation is in this repo.**

Each module exposes up to four settings, stored as control-definition
resources (type 1000, names 1–4) inside the module binary. `tools/adlib.py`
decodes them and `tools/ad_extract.py` emits a UI-ready schema. Real output,
from the shipped Flying Toasters binary:

```
Flying Toasters!  (TOASTERS.AD, hostable)
    slider   Objects:    [Flight / Squadron / Air Wing / Swarm]
    choice   Toasters:   [Adults / Babies / Random]
    slider   Music:      [Never / 1 Min. / 2 Mins. / 5 Mins. / 30 Mins. / Always]
    checkbox Display Karaok   default=True
```

### 3.1 Control types and their modern equivalents

| After Dark type | Data in the resource | Render as |
|---|---|---|
| `CTL_STRSLIDER` | N labels + N boundary values | Discrete slider with labelled stops, or a segmented control |
| `CTL_NUMSLIDER` | lower/upper/intervals + prefix or suffix text | Numeric slider with formatted value |
| `CTL_COMBOBOX` | N labels, starting index | Dropdown |
| `CTL_CHECKBOX` | default state | Toggle switch |
| `CTL_BUTTON` | title only | Button; the module handles it via `BUTTONMESSAGE` |
| `CTL_NONE` | — | Slot unused; hide it |

### 3.2 The value flow

Controls are addressed by **slot**, and the extracted schema preserves the slot
index for exactly this reason:

```
  UI widget  ->  slot value (int)  ->  AD_MODULE32.iControlValue[slot]  ->  module
```

String sliders have one subtlety worth respecting: the engine returns the
*previous* boundary value, not the one that selected the displayed label, so
the first option always yields 0. The extractor already resolves this and
emits a concrete `value` per option — the UI stores that number and never has
to model slider geometry.

`CTL_BUTTON` is the one control that requires a live module: it triggers a
module-defined dialog via `BUTTONMESSAGE` (7–10, one per slot). Route it to the
host process and let the module put up its own dialog.

### 3.3 A note on labels

`szTitle` is a fixed 14-byte field, so long labels are truncated in the binary
itself — "Display Karaok" is genuinely what the module carries. A small curated
overrides file (`data/labels.json`, keyed by module + slot) can restore full
names and add descriptions the 1996 UI had no room for. This is presentation
only; it never changes a stored value.

### 3.4 Where settings live

After Dark stored its own settings in `AFTERDRK.INI` and `MODULES.INI` (keys
`Control0`–`Control3` per module, plus `Module`, `Sleep`, `Randomizer`,
`RandomizerDuration`).

**The app should own its own settings store** — a JSON file under
`%LOCALAPPDATA%` — and inject control values into the module at runtime. It
should *not* treat the original INI files as its database:

- Writing them is only needed if you also want the original `AFTERDAR.SCR` to
  see your changes.
- Owning the store gets you per-monitor settings, presets, and multiple saved
  configurations of the same module — none of which the four-slot INI can express.

Offer *import* from the INI files on first run, and an explicit "also write
AFTERDRK.INI" compatibility toggle. One-way sync by default, in that direction.

---

## 4. The screensaver contract

`AfterDarkModern.scr` implements the standard, unchanged Windows contract:

| Argument | Meaning |
|---|---|
| `/s` | Run full-screen |
| `/p <hwnd>` | Render a preview into the tiny monitor thumbnail |
| `/c[:<hwnd>]` | Show configuration — launch the UI shell |

Registration is three values under `HKCU\Control Panel\Desktop`, written by the
app with no elevation:

```
SCRNSAVE.EXE       = <full path to AfterDarkModern.scr>
ScreenSaveActive   = "1"
ScreenSaveTimeOut  = "600"
ScreenSaverIsSecure = "0" | "1"     (require sign-in on resume)
```

Then broadcast `WM_SETTINGCHANGE` / call `SystemParametersInfo` so the running
shell picks it up. That is the entire installation. No `System32`, no `.reg`.

---

## 5. Making 1996 modules behave on modern hardware

The host process is where the era gap gets closed. Each of these is a host
concern, not a module concern — the modules are left untouched.

| Problem | Approach |
|---|---|
| **Frame pacing.** `DRAWFRAME` is called in a bare loop; on modern hardware modules run absurdly fast. | Drive `DRAWFRAME` from a fixed-timestep clock. Make the target rate a per-module setting — some modules were tuned for ~15 fps and look wrong at 60. |
| **Palettes.** Modules expect an 8-bit palettized display and a real hardware palette. | Give the module an 8-bit DIB section and its own `HPALETTE`. The host owns the composite: blit through the palette to a 32-bit surface. Modules never touch the real desktop DC. |
| **Resolution.** Authored for 640×480 / 800×600. | Render at native module resolution, then integer-scale (nearest-neighbour) to the display. Integer scaling preserves the pixel art; smooth scaling destroys it. Offer letterbox / stretch / tile. |
| **Multi-monitor.** The engine has `GetMonitorCount` / `BlankOtherMonitors`, but the era's assumptions are shaky. | Safest: one host process per monitor, each with its own module instance. Also enables different modules per display. |
| **Per-monitor DPI.** Did not exist. | Declare the host DPI-aware and do the scaling yourself; never let Windows bitmap-stretch the window. |
| **Crashes.** 30-year-old code, unusual inputs. | Already contained: the host is a separate process. On crash, log it, mark the module, fall back to blank. Never take the UI or the lock screen down. |
| **Secure resume.** AD4 called `PASSWORD.CPL`. | Do not reimplement. Set `ScreenSaverIsSecure` and let Windows handle sign-in. |

---

## 6. UI shape

- **Gallery.** All discovered modules as cards, grouped by generation
  (AD4 / Classic) with a clear badge on anything not runnable and why. Cards
  can show real artwork — modules carry `BITMAP` and `PAL` resources, readable
  without executing anything.
- **Module page.** Live preview pane, the module's own controls beside it, and
  a preset bar. Changing a control restarts or updates the preview immediately —
  the thing the original UI could never do well.
- **Presets.** Named configurations per module. "Flying Toasters / Swarm,
  babies, no music" is a preset, not a setting you rebuild each time.
- **Randomiser.** The original had one; it is a scheduler over module + preset,
  and worth keeping.
- **Health panel.** Surface the inventory tool's findings: which modules are
  hostable, which need a rewrite, whether `ADXPL510.DLL` was found.

---

## 7. Build order

Each stage ships something usable on its own.

1. **Catalogue** — wrap the existing extractors in the gallery UI. Executes no
   module code, so it cannot crash. Ships first; already largely built.
2. **Host spike** — `admhost32.exe` loading `ADXPL510.DLL` + `TOASTERS.AD` and
   getting one frame on screen. **This is the whole risk of the project.**
   Do it before writing UI polish.
3. **Configuration** — wire schema → widgets → `iControlValue[]` → live preview.
4. **Screensaver** — the `.scr`, registration, timeout, secure resume.
5. **Robustness** — multi-monitor, DPI, scaling, pacing, crash recovery.
6. **Classic coverage** — OTVDM experiment and/or native rewrites
   (see [REWRITES.md](REWRITES.md)).

### The one unknown that gates stage 2

`AD_MODULE32`'s exact field layout is not documented. The 16-bit `AD_MODULE`
layout **is** public (in the AD 3.0 SDK), and the engine exports
`PortableModule::SetUpModuleIdentity(void*, AD_MODULE32*)`, which confirms both
the name and the lineage. Recovering the layout means disassembling
`AFTERDAR.SCR` around its call into a module's `Module` export and comparing
against the known 16-bit struct. Budget a day; treat a second day as the signal
to reconsider.

---

## 8. Ground rules

- Ship **no** Berkeley Systems code, artwork, or sound. Load `ADXPL510.DLL` and
  the `.AD` files from the user's own installation at runtime.
- Never write to `System32`. Never ask for elevation. `HKCU` is sufficient.
- Never require the user to import an opaque `.reg` file.
- Read module binaries; do not modify them.
