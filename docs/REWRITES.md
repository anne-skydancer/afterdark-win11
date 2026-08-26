# Rewriting the 16-bit Classic modules as 32-bit

After Dark 4 ships 61 **Classic** modules as 16-bit NE DLLs. No 64-bit Windows
can load these in any process — not with a compatibility shim, not with a
manifest, not ever. AD4 itself ran them through a 32→16 thunk (the string
`ModuleMessage3216` is still in `AFTERDAR.SCR`), a mechanism that only ever
existed on Windows 9x.

So there are three ways to see a Classic module on Windows 11: emulate it
(OTVDM), virtualise it (a Win9x VM), or rewrite it. This document is about
rewriting.

## 0. Conversion verdict

There is no practical mechanical NE-to-PE conversion step. The 61 Classic
modules contain segmented 16-bit instructions, far pointers, NE relocations,
Win16 imports, and the AD3 message ABI. Relinking would require source and
object files; neither is present on the Deluxe disc. A binary translator able
to preserve all of those behaviours would, in substance, be a Win16 emulator.

The disc does contain Berkeley Systems' original bridge:

| File | Format | Evidence |
|---|---|---|
| `OLDMOD32.DLL` | PE32 (i386) | names `Module3216`, `ModuleMessage3216`, `ModuleCtrlValues3216` |
| `OLDMOD16.DLL` | NE (16-bit) | paired Win16 side of the bridge |

This is valuable protocol evidence, but it does not solve execution on 64-bit
Windows: the NE half still needs the Win16 subsystem that Windows 11 does not
provide. A modern equivalent must either put that half under OTVDM or replace
the module with new PE32 code.

Measured against the owned Deluxe disc:

- all **61** Classic `.AD` files are NE binaries;
- all **12** supporting binaries in the same folder are also NE;
- no C/C++, header, resource-script, object, library, or make files are present;
- module sizes and dependencies vary enormously, so conversion cost must be
  estimated per module rather than from the catalogue count alone.

The practical choices are therefore:

1. **OTVDM helper for breadth.** Run original Win16 code out of process and
   stream a DIB back to the x64 shell. This needs a dedicated compatibility
   spike; palette-heavy GDI and the old bridge are exactly the difficult cases.
2. **Native PE32 rewrites for reliability.** Implement the recovered AD4
   `Module(AD_MODULE32*)` ABI and load original resources from user-owned media.
3. **A VM as the behavioral oracle.** Useful for frame-by-frame comparison,
   but not suitable as the host Windows screensaver.

### Art Critic is not a rewrite candidate

`AD40\CRITIC.AD` (Art Critic) is already a 32-bit PE module importing
`ADXPL510.DLL`; the recovered host loads it through `AD_MODULE32` today. It
should not be patched or reimplemented merely to restore its external images.

The Deluxe disc places 11 images under `AD40\PICTURES` (BMP in this edition).
The module itself advertises `*.BMP;*.GIF;*.JPG`, exposes a `Pictures` button,
and reads this compatibility preference through Windows' INI virtualization:

```ini
[ArtCritic d29]
Art Path=C:\...\AfterDarkStudio\modules\PICTURES
```

Studio now copies only those supported image formats from owned media into
`modules\PICTURES` and seeds the path under the user's virtualized
`MODULES.INI`. A valid folder previously selected by the user is preserved.
No images enter this repository or the downloadable installer, and
`CRITIC.AD` remains byte-for-byte original.

The current minimal host still produces a blank Art Critic surface even with
the path seeded. Process Monitor confirms the module reads the nonempty INI,
but it does not enumerate images under the host's present run-mode flags. That
is a host-compatibility issue to resolve separately, not evidence that the
module needs conversion.

---

## 1. What survives the rewrite for free

A rewrite is far less speculative than it sounds, because **the parts that are
hard to recreate are all still readable from the 16-bit binary**. Only the
executable code is unusable; the resource fork is plain data.

| Asset | Where | Status |
|---|---|---|
| Sprite/frame artwork | `BITMAP`, art-block resources | Extractable |
| Palettes | `PAL` resources | Extractable |
| Sounds | `WAV`, `SOUND` resources | Extractable |
| Strings, credits | `STRINGLIST`, non-resident name table | Extractable |
| **The configuration UI** | control resources, type 1000 | **Extractable — already implemented** |

That last row is the useful surprise. `tools/ad_extract.py` reads Classic
modules as happily as AD4 ones. Straight from the shipped 16-bit `BORIS.AD`:

```
Boris  (BORIS.AD, 16-bit)
    number   Number of Cats   0..3
    slider   Butterfly:       [Never / Rarely / Seldom / Occasionally / Frequently / Always]
```

That is the rewrite's specification, recovered from the binary rather than
guessed. A reimplementation that honours those exact controls, with those exact
labels, will feel like the original in the way that matters.

**Therefore: rewrite behaviour, keep everything else.** Original artwork and
sound stay on the user's disk and get loaded at runtime — which is also the
right answer legally (see [FEASIBILITY.md](FEASIBILITY.md) §6).

---

## 2. Make rewrites drop-in

A rewritten module should be a 32-bit DLL exporting `Module`, taking the same
message codes, and carrying the same control resources. Then:

- the host from [DESIGN.md](DESIGN.md) runs originals and rewrites identically —
  no second code path;
- the configuration UI needs no special-casing;
- a rewrite can be swapped in per-module, incrementally, without any
  architectural event.

What a rewrite does *not* need is `ADXPL510.DLL`. The engine exists to provide
sprites, canvases, palettes and sound to 1996 C++; a modern rewrite brings its
own renderer and links nothing proprietary. That also makes rewrites the only
modules that work on a machine with no After Dark installed — though without
the original artwork they will need their own.

**Suggested shape:** a small shared runtime (`admkit`) providing canvas,
sprite-sheet loading from `.AD` resources, palette handling, timing and sound,
plus one source file per module. Most Classic modules are a few hundred lines
against a decent sprite layer.

---

## 3. The 61 Classic modules, by rewrite difficulty

Grouped by what the rewrite actually costs. Names are as they ship on the disc.

### Tier 1 — Pure algorithm, no artwork (~24 modules)

Deterministic procedural graphics. No sprite extraction, no asset pipeline; the
whole module is a draw loop. Several are a weekend each, and they are the right
place to start because they validate the runtime end-to-end with nothing else
in the way.

```
FRACTAL   MANDELBR  SPIRAL    STRING    WARP      ZOOM
FROST     GEOBOUNC  SHAPES    SPIN      SUNBURST  VERTIGO   ZOT
GRAVITY   REBOUND   ROSE      SPHERES   TUNNEL    PHOTON    STRANGE
GLOBE     STAINED   PUNCH     SLIDE
```

Modern rendering is a real upgrade here — these were pixel-limited, and
resolution-independent versions at 4K genuinely look better rather than merely
bigger. `MANDELBR` and `FRACTAL` in particular were compute-bound in 1996 and
are effectively free now.

### Tier 2 — Sprite animation over a simple simulation (~21 modules)

An extractable sprite sheet plus straightforward per-actor state. The work is
mostly the asset pipeline, which is written once and amortised across the tier.

```
BORIS     BADDOG3   BUGS      CONFETTI  FLOCKS    FISHPRO   WORMS     SNAKE
TOAST3    DOMINOES  MEADOW    MOUNTAIN  NIRVANA   OM        RAIN
TOILETS   DRAINO    CLOCKS3   MESSAGE3  ARTIST    MODERN
```

`FLOCKS` is boids and is well-understood. `TOAST3` is the Classic-era Flying
Toasters — worth noting that AD4's 32-bit `TOASTERS.AD` already exists and is
hostable, so this one is low priority.

`CONFETTI` was previously classified as pure algorithm. Direct inspection
shows a palette, 68 DIB resources, and three named duck resources, so it belongs
with asset-backed modules even if part of its motion is procedural. This is why
resource inventory must precede scheduling.

### Tier 3 — Complex behaviour or genuine game logic (~16 modules)

Real state machines, scripted sequences, or interactivity. These need
observation of the original — in a VM or under OTVDM — not just extraction.

```
LUNATIC   RATRACE   MOWIN     DAREDEVI  BOGGLINS  PUZZLE    YBYH
NONSENSE  SATORI    WMORPH    RAY       SPOT      EINSTEIN  DOSSHELL
MARBLES2  NOCTURNE
```

`LUNATIC` (Lunatic Fringe) is a playable vector-graphics shooter — a game, not
a screensaver, and a project of its own. `RATRACE` and `MOWIN` have elaborate
scripted behaviour. `DOSSHELL` and `EINSTEIN` are fake-UI gags whose comedy
depends on mimicking a 1996 screen; they may be better as period pieces than as
modernised versions.

*Tier assignment is a planning estimate from module identity and resource
composition, not from disassembly. Extract each module's controls and resources
first — that is cheap, and it will move some modules between tiers.*

---

## 4. A practical order of work

1. **Build the runtime against Mandelbrot. Done.** `MANDELBR.AD` is 25,312 bytes,
  imports only Win16 system APIs plus `WIN87EM`, and has no bitmap, palette,
  sound, or custom art resources. Its recovered controls are `Delay`
  (`0 sec.` through `1 min.`) and `Colors` (`Earth`, `Air`, `Fire`, `Water`,
  `Random`). It proves timing, canvas, scaling and the config path with no
  asset pipeline in the way. The independent `MANDEL32.AD` pilot now builds
  as PE32 i386, exports `Module`, embeds the original control schema, and
  renders through the recovered block without `ADXPL510.DLL`.
2. **Add a second procedural module. Done.** `SHAPES.AD` is a 6,864-byte NE
  module with no bitmap, palette, sound, or art resources. `SHAPES32.AD`
  preserves its `Clear Screen Frequently` and `Color` toggles and renders
  independent accumulating geometric outlines through the same ABI.
3. **Add a shared procedural runtime and three-module batch. Done.** `admkit`
  now provides the bounded canvas primitives shared by `SPIRAL32.AD`,
  `TUNNEL32.AD`, and `ZOT32.AD`. All three preserve their recovered controls,
  embed independent palettes, and contain no original assets.
4. **Expand the asset-free batch. Done.** `WARP32.AD`, `SPHERES32.AD`, and
  `STAINED32.AD` use the same runtime for perspective stars, accumulated
  circles, and filled stained-glass cells. Their controls match the originals
  field-for-field and their generated palettes contain no original data.
5. **Add line and particle effects. Done.** `STRING32.AD`, `PHOTON32.AD`, and
  `STRANGE32.AD` add accumulated string art, persistent particle bursts, and
  nonlinear attractors. Timed controls use wall-clock durations and the host
  bounds engine-free render surfaces before scaling to 4K presentation.
6. **Add moving particle modules. Done.** `RAIN32.AD`, `SPOT32.AD`, and
  `DRAINO32.AD` add slanted rain, independently moving spotlights, and a
  configurable drain spiral. Their original modules contain no artwork or
  sound resources, and all controls match field-for-field.
7. **Add procedural geometry modules. Done.** `MOUNTAIN32.AD`, `VERTIGO32.AD`,
  and `SUNBURST32.AD` add projected terrain meshes, rotating spirals, and
  wall-clock-paced radial rays. Mountain modes and complexity levels produce
  distinct geometry, and all controls match field-for-field.
8. **Add abstract simulations. Done.** `SATORI32.AD`, `SNAKE32.AD`, and
  `NIRVANA32.AD` add mixed pattern fields, generated perfect mazes with solved
  paths, and persistent flow fields. All controls match field-for-field.
9. **Add clean-room physics. Done.** `GRAVITY32.AD`, `PUNCH32.AD`, and
  `WORMS32.AD` add bouncing gravity balls, expanding geometric outlines, and
  coherent segmented worms. Their original custom data is not copied; all
  visuals and simulation are independent.
10. **Add period and utility scenes. Done.** `DOSSHELL32.AD`, `PUZZLE32.AD`, and
  `GLOBE32.AD` add a scalable DOS file manager, legal sliding-puzzle movement,
  and a rotating wireframe globe with a functional Map button.
11. **Build the asset pipeline against one Tier 2 module.** `BORIS` is a good
  target: two controls, recovered above, and a recognisable result that makes
  fidelity easy to judge.
12. **Then breadth.** With runtime and pipeline done, Tier 1 becomes a
   production line and Tier 2 becomes routine.
13. **Tier 3 only by demand.** Each is a bespoke project; pick the ones people
   actually miss.

A reasonable v1 target is Tier 1 plus a handful of beloved Tier 2 modules —
roughly half the Classic set, at a fraction of the total effort.

---

## 5. Fidelity: how close is close enough?

Worth deciding deliberately, per module, rather than drifting:

- **Faithful** — original pixel dimensions, original palette, original frame
  rate, integer-scaled. Feels exactly like 1996.
- **Remastered** — original artwork and behaviour, but resolution-independent
  motion, smooth interpolation, high refresh.

These conflict more than they first appear. Smoothly interpolated Flying
Toasters are objectively nicer and subjectively wrong. The honest answer is to
make it a per-module setting with a faithful default, and let the module's own
controls sit alongside it — which the four-slot original format could never
have expressed, and the modern UI can.

---

## 6. Host work required for rewrites

`admhost32` now attempts to load `ADXPL510.DLL`, then continues when it is
absent so a self-contained rewrite can bind. Original AD4 modules still fail
clearly when their engine imports cannot resolve; rewrites need no special
host path or proprietary runtime.

```
make rewrite
admhost32 dist/rewrites dist/rewrites/MANDEL32.AD --frames 3
admhost32 dist/rewrites dist/rewrites/SHAPES32.AD --frames 80 --controls 0,1,0,0
admhost32 dist/rewrites dist/rewrites/SPIRAL32.AD --frames 30 --controls 54,28,32,0
admhost32 dist/rewrites dist/rewrites/TUNNEL32.AD --frames 30 --controls 0,1,0,0
admhost32 dist/rewrites dist/rewrites/ZOT32.AD --frames 40 --controls 67,0,75,0
admhost32 dist/rewrites dist/rewrites/WARP32.AD --frames 30 --controls 0,71,0,0
admhost32 dist/rewrites dist/rewrites/SPHERES32.AD --frames 80 --controls 73,27,30,0
admhost32 dist/rewrites dist/rewrites/STAINED32.AD --frames 20 --controls 10,100,100,0
admhost32 dist/rewrites dist/rewrites/STRING32.AD --frames 20 --controls 2,87,96,0
admhost32 dist/rewrites dist/rewrites/PHOTON32.AD --frames 40 --controls 58,0,1,0
admhost32 dist/rewrites dist/rewrites/STRANGE32.AD --frames 20 --controls 30,66,0,0
admhost32 dist/rewrites dist/rewrites/RAIN32.AD --frames 40 --controls 4,18,0,0
admhost32 dist/rewrites dist/rewrites/SPOT32.AD --frames 30 --controls 0,10,2,0
admhost32 dist/rewrites dist/rewrites/DRAINO32.AD --frames 60 --controls 67,0,1,1
admhost32 dist/rewrites dist/rewrites/MOUNTAIN32.AD --frames 4 --controls 5,9,5,70
admhost32 dist/rewrites dist/rewrites/VERTIGO32.AD --frames 20 --controls 2,100,100,35
admhost32 dist/rewrites dist/rewrites/SUNBURST32.AD --frames 30 --controls 60,0,0,0
admhost32 dist/rewrites dist/rewrites/SATORI32.AD --frames 30 --controls 6,13,0,7
admhost32 dist/rewrites dist/rewrites/SNAKE32.AD --frames 80 --controls 1,0,0,0
admhost32 dist/rewrites dist/rewrites/NIRVANA32.AD --frames 60 --controls 7,57,0,0
admhost32 dist/rewrites dist/rewrites/GRAVITY32.AD --frames 40 --controls 4,24,1,0
admhost32 dist/rewrites dist/rewrites/PUNCH32.AD --frames 40 --controls 0,65,9,0
admhost32 dist/rewrites dist/rewrites/WORMS32.AD --frames 40 --controls 69,15,15,0
admhost32 dist/rewrites dist/rewrites/DOSSHELL32.AD --frames 30 --controls 0,1,75,0
admhost32 dist/rewrites dist/rewrites/PUZZLE32.AD --frames 30 --controls 0,0,0,0
admhost32 dist/rewrites dist/rewrites/GLOBE32.AD --frames 30 --controls 8,53,0,0 --button 2
```

All rewrites receive the same 348-byte block and lifecycle as an original
module. They carry generated type-1000 resources; catalogue and settings code
require no rewrite-specific branch. Each also carries an independent PAL
resource. Measured host output is nonblank at both 32 bpp and the
production-default 8 bpp.

Future asset-backed rewrites should receive a read-only source path for the
user's original resources. A shared open-source `admkit` should own ABI
dispatch, palette/DIB decoding, timing, scaling, and sound so later modules
contain only their behavior.

The first acceptance test should compare deterministic Mandelbrot frames and
control values against a Win16 reference run. Only after that should work begin
on the shared asset pipeline and BORIS.

## 7. The alternative: don't rewrite

Before committing to broad rewrites, spend a focused spike on OTVDM with
Mandelbrot, BORIS, and one ADXPL300-dependent module. OTVDM's latest formal
release at the time of this study is v0.9.0 (September 2023), so treat it as an
optional external compatibility layer rather than a bundled foundation.
Palette-heavy DIB code is exactly the hard case for Win16 emulation, so it may
go badly — but if even half the Classic set runs acceptably, that is 30 modules
for days of integration rather than months of rewrites.

The likely best outcome is both: OTVDM for breadth, native rewrites for the
dozen modules people actually care about.
