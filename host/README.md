# admhost32 — a working host for After Dark 4 modules

The stage-2 spike from [../docs/DESIGN.md](../docs/DESIGN.md), and the proof
that the [recovered ABI](../docs/ABI.md) is right: it loads your own
`ADXPL510.DLL` and an `.AD` module, drives the lifecycle, and renders real
frames to a `.BMP`.

## Status: it works

Verified against two shipped After Dark 4 modules. Every message returned
`AD_OK`; both produced their real artwork:

| Module | Result |
|---|---|
| `TOASTERS.AD` | "Late Jurassic" intro banner, then flying toasters and toast |
| `BADDOG.AD` | the fake Windows 95 desktop, dog digging, joke folder names |

The configuration path was verified too. `--controls 75,0,0,0` sets the
"Objects:" slider to **Swarm** (the value the
[extractor](../tools/ad_extract.py) reads out of the module's own control
resource) and produces 3.5× the on-screen density of the default **Flight**.
Schema → `iControlValue[0]` → module behaviour, end to end.

**Caveat on the verification.** This was run under Wine 9.0 on Linux, because
that is the machine it was developed on. The module binaries, the engine and
the ABI are all real, so the calling convention, block layout, message
numbering and lifecycle are confirmed. A run on Windows 11 itself is still the
final word — and is the one thing this spike cannot self-certify.

## Build

```
make                      # MinGW cross-compile
cl /O2 admhost32.c user32.lib gdi32.lib     # MSVC, 32-bit toolchain
```

The program refuses to run if built 64-bit or if `AD_MODULE32` is the wrong
size, rather than failing mysteriously later.

## Run

```
admhost32 <install-dir> <module.AD> [options]

  --frames N           DRAWFRAME iterations (default 60)
  --fps N              frame pacing; 0 = unpaced, like the original (default 30)
  --size WxH           surface size (default 640x480)
  --bpp 8|32           surface depth (default 8)
  --controls a,b,c,d   iControlValue[0..3]
  --bmp FILE           write the final surface
  --present            show a window and run until input (screensaver mode)
  --parent HWND        render inside an existing window; implies --present
  --stream             write raw frames to stdout for a UI to display
  --scale MODE         integer (default) or stretch
```

### --stream

Feeds the Studio preview pane. Frames go to stdout, diagnostics to stderr:

```
magic 4 "ADFS" | version 4 | width 4 | height 4 | bpp 4 | stride 4
palette 1024 (256 x BGRA, zeroed at 32bpp)
---- then frames of stride*height, back to back ----
```

Fixed-size frames mean the reader needs no framing, and a module that dies is
simply end-of-stream. The reader should scan for the magic rather than assume
offset 0 — the CRT or the module can put something on stdout first.

Example — a swarm of toasters, 30 seconds at 30 fps:

```
admhost32 "C:\Program Files (x86)\After Dark" TOASTERS.AD \
          --frames 900 --fps 30 --controls 75,0,0,0 --bmp swarm.bmp
```

## Things it proved that matter for the real host

- **`+0x18` is just an `HDC`.** Modules draw into whatever DC you hand them, so
  an offscreen DIB section works. That is the isolation the design wanted for
  palette handling and scaling — confirmed, not assumed.
- **Pacing is entirely the host's business.** The original loop is an unpaced
  spin. `--fps 0` reproduces it; anything else is an improvement the module
  neither knows nor cares about. Note the intro banner is timed off wall clock,
  so unpaced frames never get past it.
- **You must install the module's own palette.** Modules carry a `LOGPALETTE`
  in a `PAL` resource. Without it you get a correct image in wrong colours —
  `SetDIBColorTable` with those entries fixes it.
- **Sound is attempted at `PREINITIALIZE`**, so an audio path needs handling
  (or silencing) early.
- **Send one `PAINT` before the frame loop.** Modules repaint only damaged
  rectangles per `DRAWFRAME`, so anything they have not touched keeps whatever
  the surface started as — palette index 0, near-white in most modules. This
  showed up as a large white band in both the screensaver and the preview and
  was easy to misread as a capture artifact. The `--bmp` path never showed it
  because it sends `PAINT` before saving.

## What it is not

A screensaver, a UI, or multi-monitor aware. It is one process that proves the
hard part works. The real host adds frame pacing policy, scaling, IPC to the
64-bit shell, and crash recovery — see [../docs/DESIGN.md](../docs/DESIGN.md).

## Content

Ships no Berkeley Systems code or artwork. Everything is loaded at runtime from
an installation you already own.
