# Native Classic rewrites

Independent 32-bit PE modules that implement the recovered `AD_MODULE32` ABI.
They contain no Berkeley Systems code, artwork, or sound.

## Mandelbrot pilot

`mandelbrot32.c` is the first procedural pilot. It preserves the original
Mandelbrot module's two controls:

- Delay: 0 sec., 5 sec., 15 sec., 30 sec., 1 min.
- Colors: Earth, Air, Fire, Water, Random

Build it with:

```sh
make rewrite
```

The result is `dist/rewrites/MANDEL32.AD`. The build generates the documented
control-definition resources, compiles them with `windres`, and links a
self-contained x86 DLL. It does not need `ADXPL510.DLL` or the original
16-bit module.

The pilot validates the drop-in contract: catalogue discovery, control slots,
module lifecycle, and drawing through the HDC in `AD_MODULE32`. Behavioral
comparison against the original under Win16 emulation remains a separate
fidelity step.

Validated on Windows 11:

- PE32 i386 DLL with undecorated `Module` export
- no `ADXPL510.DLL` dependency
- both controls decoded identically by the Python and managed catalogues
- full lifecycle through `admhost32` without an engine
- nonblank 32-bit output and palette-backed 8-bit output
