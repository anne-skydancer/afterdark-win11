# Rewriting the 16-bit Classic modules as 32-bit

After Dark 4 ships 61 **Classic** modules as 16-bit NE DLLs. No 64-bit Windows
can load these in any process — not with a compatibility shim, not with a
manifest, not ever. AD4 itself ran them through a 32→16 thunk (the string
`ModuleMessage3216` is still in `AFTERDAR.SCR`), a mechanism that only ever
existed on Windows 9x.

So there are three ways to see a Classic module on Windows 11: emulate it
(OTVDM), virtualise it (a Win9x VM), or rewrite it. This document is about
rewriting.

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

### Tier 1 — Pure algorithm, no artwork (~25 modules)

Deterministic procedural graphics. No sprite extraction, no asset pipeline; the
whole module is a draw loop. Several are a weekend each, and they are the right
place to start because they validate the runtime end-to-end with nothing else
in the way.

```
CONFETTI  FRACTAL   MANDELBR  SPIRAL    STRING    WARP      ZOOM
FROST     GEOBOUNC  SHAPES    SPIN      SUNBURST  VERTIGO   ZOT
GRAVITY   REBOUND   ROSE      SPHERES   TUNNEL    PHOTON    STRANGE
GLOBE     STAINED   PUNCH     SLIDE
```

Modern rendering is a real upgrade here — these were pixel-limited, and
resolution-independent versions at 4K genuinely look better rather than merely
bigger. `MANDELBR` and `FRACTAL` in particular were compute-bound in 1996 and
are effectively free now.

### Tier 2 — Sprite animation over a simple simulation (~20 modules)

An extractable sprite sheet plus straightforward per-actor state. The work is
mostly the asset pipeline, which is written once and amortised across the tier.

```
BORIS     BADDOG3   BUGS      FLOCKS    FISHPRO   WORMS     SNAKE
TOAST3    DOMINOES  MEADOW    MOUNTAIN  NIRVANA   OM        RAIN
TOILETS   DRAINO    CLOCKS3   MESSAGE3  ARTIST    MODERN
```

`FLOCKS` is boids and is well-understood. `TOAST3` is the Classic-era Flying
Toasters — worth noting that AD4's 32-bit `TOASTERS.AD` already exists and is
hostable, so this one is low priority.

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

1. **Build the runtime against one Tier 1 module.** `CONFETTI` or `GEOBOUNC`.
   Proves timing, canvas, scaling and the config path with no asset pipeline in
   the way.
2. **Build the asset pipeline against one Tier 2 module.** `BORIS` is a good
   target: two controls, recovered above, and a recognisable result that makes
   fidelity easy to judge.
3. **Then breadth.** With runtime and pipeline done, Tier 1 becomes a
   production line and Tier 2 becomes routine.
4. **Tier 3 only by demand.** Each is a bespoke project; pick the ones people
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

## 6. The alternative: don't rewrite

Before committing to any of this, spend an afternoon on OTVDM with a handful of
Classic modules. Palette-heavy DIB code is exactly the hard case for Win16
emulation, so it may go badly — but if even half the Classic set runs
acceptably, that is 30 modules for a day's work versus months.

The likely best outcome is both: OTVDM for breadth, native rewrites for the
dozen modules people actually care about.
