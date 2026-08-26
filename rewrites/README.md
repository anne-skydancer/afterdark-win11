# Native Classic rewrites

Independent 32-bit PE modules that implement the recovered `AD_MODULE32` ABI.
They contain no Berkeley Systems code, artwork, or sound.

## Procedural modules

`mandelbrot32.c` is the first procedural rewrite. It preserves the original
Mandelbrot module's two controls:

- Delay: 0 sec., 5 sec., 15 sec., 30 sec., 1 min.
- Colors: Earth, Air, Fire, Water, Random

`shapes32.c` preserves the original Shapes module's two toggles:

- Clear Screen Frequently: off by default
- Color: on by default

It accumulates independently drawn rectangles, ellipses, triangles, diamonds,
and crossed lines. An independent 256-entry palette preserves its eight colors
on the host's production-default 8-bit surface.

The next procedural batch shares the small `admkit` pixel-canvas runtime:

- `SPIRAL32.AD` preserves Max Lines, Min Lines, and Color Cycling while drawing
	animated rotating chord fields.
- `TUNNEL32.AD` preserves Direction and Shape choices while drawing inward or
	outward square, rounded, or alternating nested geometry.
- `ZOT32.AD` preserves Forkiness and How Often while drawing fading branched
	lightning.

`admkit` owns only allocation, bounded pixel primitives, palette colors,
randomness, fading, and HDC presentation. Module lifecycle and behavior remain
in each module source file.

The second `admkit` batch adds three more asset-free modules:

- `WARP32.AD` preserves Speed, Stars, Size, and Color while drawing an inward
	or outward perspective star field.
- `SPHERES32.AD` preserves Max Size, Offset, Clear Every, and Clear Screen
	Frequently while accumulating highlighted circle outlines.
- `STAINED32.AD` preserves Complexity, Duplication, and Color while drawing
	animated, repeated stained-glass cells with black leading.

Build it with:

```sh
make rewrite
```

The eight results are written below `dist/rewrites`. The build generates the
documented control-definition resources, compiles them with `windres`, and
links self-contained x86 DLLs. They do not need `ADXPL510.DLL` or the original
16-bit modules. `make dist` builds them, and future installer builds place them
under `modules/rewrites` without bundling any original module or media.

The rewrites validate the drop-in contract: catalogue discovery, control slots,
module lifecycle, and drawing through the HDC in `AD_MODULE32`. Behavioral
comparison against the original under Win16 emulation remains a separate
fidelity step.

Validated on Windows 11:

- PE32 i386 DLL with undecorated `Module` export
- no `ADXPL510.DLL` dependency
- all controls decoded from generated type-1000 resources
- full lifecycle through `admhost32` without an engine
- nonblank 32-bit output and palette-backed 8-bit output
- WPF catalogue, generated controls, and streamed live preview
