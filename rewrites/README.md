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

The third batch covers line and particle effects:

- `STRING32.AD` preserves String Groups, Strings, Color Speed, and Clear Screen
	Frequently while accumulating circular string-art chords.
- `PHOTON32.AD` preserves Length, Burst Delay, Always Centered, and Burst while
	drawing persistent radial particle bursts.
- `STRANGE32.AD` preserves Duration and Color Speed while tracing changing
	nonlinear attractors.

Photon delays and Strange durations use wall-clock time rather than assuming a
fixed frame rate. Engine-free surfaces larger than 1920x1080 are rendered at a
bounded aspect-preserving size and scaled by the host for smooth 4K output.
Every generated module carries an `AD_REWRITE` marker resource, so the host can
apply that policy even when `ADXPL510.DLL` is present in the same installation.

The fourth batch adds three more asset-free particle modules:

- `RAIN32.AD` preserves drop count, drop size, and Clear Screen Frequently
	while drawing slanted multicolor rain with optional trails.
- `SPOT32.AD` preserves exact discrete Size stops, Speed, and Spots while
	moving independently colored spotlights around the surface.
- `DRAINO32.AD` preserves Speed, Direction, Drops, and Show Drain while drawing
	particles spiraling into a visible drain.

The fifth batch adds three asset-free geometry modules:

- `MOUNTAIN32.AD` preserves View, Planet, Complexity, and Zoom while drawing
	complete projected height-field meshes with distinct boundary, web,
	mountain, construction, and highland modes.
- `VERTIGO32.AD` preserves Palette, Spiral Pitch, Color Speed, and Delay while
	drawing rotating multicolor spirals with randomized palette scenes.
- `SUNBURST32.AD` preserves Delay while drawing a wall-clock-paced expanding
	radial field.

The sixth batch adds abstract simulations:

- `SATORI32.AD` preserves Display, Colors, End Clarity, and Knots while drawing
	mixed fields, pools, rays, waves, and leaf patterns.
- `SNAKE32.AD` preserves Solution Speed, Maze Complexity, and Pause When Done;
	it generates a perfect maze and progressively reveals its solved path.
- `NIRVANA32.AD` preserves Color, Redraw Every, Activity, and Change Color while
	tracing a persistent flow field using independent palette data.

Build it with:

```sh
make rewrite
```

The twenty results are written below `dist/rewrites`. The build generates the
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
