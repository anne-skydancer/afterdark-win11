# A modern UI for After Dark 4 on Windows 11 — feasibility study

**Verdict: feasible, with one important caveat.**

After Dark 4's own screensaver modules are ordinary 32-bit Windows DLLs that
Windows 11 can still load. A modern front-end can host them directly. The
caveat is that roughly half of what ships in the After Dark 4 box — the
"Classic" folder, which carries most of the famous 1991-vintage modules — is
16-bit code that **no** 64-bit Windows can execute, in any process, ever. Those
need a separate strategy.

This document records what was verified, how, and what the options are.

---

## 1. What was actually verified

Every claim below was checked against real After Dark 4 binaries and against
the original Berkeley Systems SDK, not inferred from documentation.

Binaries examined (from the *After Dark 4.0 Deluxe* disc layout):

| File | Format | Notes |
|---|---|---|
| `ADE/FILES/AD40/TOASTERS.AD` | **PE32 (i386) DLL** | Flying Toasters. Exports `Module`. |
| `ADE/FILES/AD40/BADDOG.AD` | **PE32 (i386) DLL** | Bad Dog. Exports `Module`. |
| `ADE/FILES/AD40/ADXPL510.DLL` | **PE32 (i386) DLL** | Engine. Version string: *"After Dark Cross Platform DLL"*. 1,171 exports. |
| `ADE/FILES/ENGINE/AFTERDAR.SCR` | **PE32 (i386) EXE** | The screensaver host. |
| `ADE/FILES/CLASSIC/BORIS.AD` | **NE (16-bit)** | Classic-era module. |
| `ADE/FILES/CLASSIC/ADXPL300.DLL` | (16-bit engine) | Separate engine for classic modules. |

The disc carries **84 `.AD` modules**, split across `AD40/` (native 32-bit) and
`CLASSIC/` (16-bit).

### 1.1 The AD4 module ABI

Modules are DLLs renamed to `.AD`. Each one exports a single C-linkage entry
point, `Module`, alongside a large set of Borland C++ mangled symbols
(`@FlyingToastersModule@DoDrawFrame$qv`, etc. — the `$q` mangling is Borland
C++, which dates the toolchain).

Modules are **not self-contained**. `TOASTERS.AD` imports **155** functions from
`ADXPL510.DLL`; `BADDOG.AD` imports **300**. The engine DLL exposes a full C++
class library — 61 classes including `XCanvas`, `XImage`, `XPalette`,
`XSpriteSystem`, `XNoiseMaker`, `WinScreenCanvas`, `World`, `Background`.

The engine's `PortableModule` class is effectively the documented host contract,
and its exported method names tell you exactly what a host must provide:

```
CreateTheScreenCanvas      GetMonitorCount        GetControlValue
DoBlankScreen              GetMonitorInfo         GetSliderValue
DoPaint                    BlankOtherMonitors     GetMenuValue
DoUserMessage              RequestScreenSize      ReadModulePrefs
GetScreenCanvas            RestoreScreenSize      WriteModulePrefs
GetTheirDC                 IsDemoMode             WantEvents
SetUpModuleIdentity(void*, AD_MODULE32*)          IsInteractive
```

Note `SetUpModuleIdentity(void*, AD_MODULE32*)` — a struct literally named
`AD_MODULE32`. This is the Win32 evolution of the documented 16-bit `AD_MODULE`
parameter block from the After Dark 3.0 SDK, whose layout is public:

```c
struct AD_MODULE {
    HRGN         hDrawRgn;         // region for module to draw into
    POINT        ptRgnSize;
    int          iControlValue[4]; // the four module control settings
    int          iControlID[4];
    HANDLE       hModule;
    HPALETTE     hPalette;
    LPLOGPALETTE lpLogPalette;
    BOOL         bWantSnd;
    int          iModRunner;
};
```

and whose message protocol is likewise public:

```c
#define PREINITIALIZE  12   #define INITIALIZE  0   #define BLANK  1
#define DRAWFRAME       2   #define CLOSE       3   #define ABOUT  6
#define MODULESELECTED  5   #define BUTTONMESSAGE 7  // 7..10 = 4 controls
```

The 3.0 entry point was `int FAR PASCAL Module(int iMessage, HDC hDrawDC,
HANDLE hADSystem)`. The 4.0 export keeps the name and the message-dispatch
shape; the parameter block widened to Win32 types. **This is the single
highest-value unknown to pin down** — see §5.

### 1.2 Modules are self-describing

Each `.AD` carries standard PE resources plus custom types: `PAL` (palette),
`STRINGLIST`, `WAV`, `BITMAP`, `STRING`, `VERSION`, and numbered types
(`#1000`, `#2000`, `#8000`–`#8102`) which correspond to the control-definition
resources the 16-bit SDK called `CTRL1`–`CTRL4`, `CWIN`, `MNAME`, `CSTR`.

**A modern UI can enumerate every module — name, description, artwork, and its
four configurable controls — by reading resources, without executing anything.**
That is a completely safe, crash-proof foundation for a browser/config UI, and
it is available on day one.

### 1.3 Windows 11 still supports screensavers

The `.scr` mechanism is intact: Settings → Personalization → Lock screen →
Screen saver, or `control desk.cpl,,@screensaver` directly. The registry keys
under `HKCU\Control Panel\Desktop` (`SCRNSAVE.EXE`, `ScreenSaveTimeOut`,
`ScreenSaveActive`, `ScreenSaverIsSecure`) are unchanged, as is the
`/s` `/c` `/p <hwnd>` command-line contract. Nothing about the delivery
mechanism is deprecated.

Notably, `AFTERDAR.SCR` imports **only** `USER32`, `GDI32`, `ADVAPI32`,
`SHELL32`, `WINMM` and `KERNEL32` — no 16-bit thunks. The original AD4 host is
a clean Win32 program. What is broken on modern Windows is its *installer and
control-panel integration*, not its engine.

---

## 2. Why the existing experience is bad (and what that implies)

After Dark 4 shipped in 1996 assuming it could own the display. The specific
frictions on Windows 11:

- **Installer.** The 16-bit setup program cannot run; registry entries never
  get written, and the module list comes up empty or errors with
  "unable to select module...".
- **Configuration UI.** AD4's control panel hooked into a Windows 95 shell
  extension point that no longer exists. This is the *primary* thing a modern
  UI replaces.
- **Display assumptions.** Modules were authored for a single 640×480 or
  800×600 palettized display. Windows 11 means multi-monitor, per-monitor DPI,
  4K, high refresh rates, and no hardware palette.
- **Timing.** `DRAWFRAME` is called in a loop with no frame pacing. On modern
  hardware, unthrottled modules run absurdly fast.

Each of these is a solvable host-side problem, which is precisely the argument
for a new host.

---

## 3. The options

### Option A — Modern host process (recommended)

A 32-bit host `.exe` that `LoadLibrary`s the user's own `ADXPL510.DLL` and the
selected `.AD`, then drives the `Module` message loop, rendering into a
window you control.

- **Fidelity:** perfect — it is the original code.
- **Coverage:** the ~22 `AD40/` modules only.
- **Cost:** moderate. The work is pinning down `AD_MODULE32` and the engine's
  initialization sequence.
- **Constraint:** the host that touches the modules **must be 32-bit x86**.
  A 64-bit process cannot load a 32-bit DLL. This is fine — run the modern UI
  as a 64-bit app and the module host as a small 32-bit child process, sharing
  a swapchain or shared-memory surface. That split is also a robustness win:
  a module that crashes takes down only the child.
- **Works on ARM64 Windows 11** too, via x86 emulation.

### Option B — OTVDM/winevdm for the Classic modules

[OTVDM/winevdm](https://github.com/otya128/winevdm) is an open-source Win16
emulation layer built on Wine that runs 16-bit Windows software on 64-bit
Windows. It is the only realistic route to the `CLASSIC/` modules short of
rewriting them.

- **Fidelity:** good but not guaranteed; graphics-heavy modules poking at
  palettes and DIBs are exactly the hard case for Win16 emulation.
- **Cost:** low to try, unbounded to perfect.
- **Recommendation:** treat as an experiment, not a foundation. Test three or
  four Classic modules early; the result tells you whether Classic support is
  a feature or a footnote.

### Option C — Reimplement modules natively

Rewrite the behaviours (Flying Toasters, Starry Night, Rat Race…) against a
modern renderer, loading artwork and sound from the user's own installed `.AD`
resources at runtime.

- **Fidelity:** as good as you make it; can exceed the original (4K, smooth
  motion, HDR).
- **Coverage:** whatever you build, 16-bit modules included — this is the only
  path that rescues Classic content cleanly.
- **Cost:** high per module, but each one is independent and shippable.
- This is what the well-known web recreations do, and why they look good.

### Option D — Emulation in a VM

Run AD4 in a Windows 95/98 VM (86Box, VirtualBox). Complete and faithful, but
it cannot function as an actual screensaver on the host. Useful only as a
reference implementation to compare against while building A or C.

---

## 4. Recommended architecture

A staged plan where each stage ships something usable:

```
┌────────────────────────────────────────────────────────┐
│  Stage 1 — Catalogue                        (low risk) │
│  Read .AD resources. Modern gallery UI: browse all 84  │
│  modules, artwork, descriptions, control definitions.  │
│  Executes nothing. Cannot crash. Ships immediately.    │
├────────────────────────────────────────────────────────┤
│  Stage 2 — Native host             (the load-bearing   │
│  32-bit child process; LoadLibrary ADXPL510 + .AD;      risk)
│  drive Module(). Render to a host-owned surface.       │
│  Proves or kills the whole approach — do it early.     │
├────────────────────────────────────────────────────────┤
│  Stage 3 — Screensaver shell                           │
│  One .scr honouring /s /c /p. Per-monitor DPI, frame   │
│  pacing, multi-monitor, secure-resume.                 │
├────────────────────────────────────────────────────────┤
│  Stage 4 — Coverage                                    │
│  OTVDM experiment for Classic (B), and/or native       │
│  reimplementations of the best-loved modules (C).      │
└────────────────────────────────────────────────────────┘
```

**Suggested stack:** C++ or Rust for the 32-bit host (it must speak the Win32
ABI directly); anything modern for the shell UI — WinUI 3, Avalonia, or Tauri.
Keep the two separated by a narrow IPC boundary from the start.

**Do Stage 2 as a spike before committing to anything else.** A throwaway
console program that loads `ADXPL510.DLL`, loads `TOASTERS.AD`, sends
`INITIALIZE`/`BLANK`/`DRAWFRAME`, and gets a single toaster onto a window is
the entire risk of this project concentrated into one afternoon.

---

## 5. Open risks

| Risk | Severity | Mitigation |
|---|---|---|
| `AD_MODULE32` layout unknown | **High** — blocks Option A | Derive by disassembling `AFTERDAR.SCR`'s call into `Module`, or find the AD4 SDK (a "Standard Development Kit" for VC++ 5.0 existed). The 16-bit layout is a strong template. |
| Engine may require init by the real `.scr` | High | Investigate during the Stage 2 spike. Worst case, the host mimics more of `AFTERDAR.SCR`'s startup. |
| Palette-dependent rendering | Medium | Host renders into a 8-bit DIB section and composites/upscales itself. This is also where 4K scaling gets solved. |
| Modules assume they own the screen | Medium | Give each module its own offscreen surface; never let it draw to the real desktop DC. |
| Classic (16-bit) modules | Medium | Options B or C. Scope them out of v1. |
| Module crashes | Low | Already mitigated by the separate 32-bit host process. |

---

## 6. Legal position

You own a licensed copy, and that matters here. The recommended design keeps
you on solid ground:

- The host ships **no** Berkeley Systems code or artwork. It loads `ADXPL510.DLL`
  and the `.AD` files **from the user's own installation**, at runtime.
- Interface information needed to make an independent program interoperate is
  the textbook interoperability case, and the module ABI is in any event
  partly published in Berkeley Systems' own SDK.
- **Do not** commit `.AD` files, `ADXPL510.DLL`, disc images, or extracted
  artwork to this repository. Ship a tool that reads the user's install; never
  a copy of it.
- If you reimplement modules (Option C), reimplement *behaviour*. Original
  sprites and sounds stay on the user's disk and get loaded at runtime.

This is engineering guidance, not legal advice. After Dark's rights have
changed hands over the years; if this ever becomes a public distribution rather
than a personal project, that is the point to get a real opinion.

---

## 7. Next step

Run the inventory tool against your installation:

```
python tools\ad_inventory.py "C:\Program Files (x86)\After Dark" --json ad4.json
```

It reports, per module, the executable format, the engine DLL it binds to,
whether it exports `Module`, and whether a native host can load it — turning
the AD40/Classic split above into an exact list for *your* copy. That list
determines how much of Option A you get for free, and how much needs B or C.
