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

Build it with:

```sh
make rewrite
```

The results are `dist/rewrites/MANDEL32.AD` and
`dist/rewrites/SHAPES32.AD`. The build generates the documented
control-definition resources, compiles them with `windres`, and links
self-contained x86 DLLs. They do not need `ADXPL510.DLL` or the original
16-bit modules.

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
