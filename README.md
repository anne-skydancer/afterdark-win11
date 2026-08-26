# afterdark-win11

Exploring a modern front-end for **After Dark 4** screensavers on Windows 11.

## Status

Feasibility study complete. **Verdict: feasible**, with a split outcome:

- After Dark 4's own (`AD40`) modules are 32-bit PE DLLs exporting a `Module`
  entry point, bound to the `ADXPL510.DLL` engine. Windows 11 can still load
  these — a 32-bit host process can drive them directly.
- The bundled **Classic** modules are 16-bit NE DLLs. No 64-bit Windows can
  load these in any process. They need OTVDM/winevdm, or reimplementation.

Read **[docs/FEASIBILITY.md](docs/FEASIBILITY.md)** for the evidence, the
module ABI, the options, and a staged plan.

## Inventory your installation

```
python tools\ad_inventory.py "C:\Program Files (x86)\After Dark"
```

Reports each module's executable format, engine dependency, entry point and
whether a native host can load it. Pure stdlib Python 3, no dependencies,
read-only — it parses binaries, it never executes them.

Add `--json out.json` for full detail.

Example output:

```
MODULE       FORMAT                ENGINE          HOST?
-----------------------------------------------------------
BADDOG.AD    PE32 (i386)           adxpl510.dll    yes
TOASTERS.AD  PE32 (i386)           adxpl510.dll    yes
BORIS.AD     NE (16-bit Windows)   AD_RSRC, KERN…  NO (16-bit)
```

## A note on content

This repository contains no Berkeley Systems code or artwork, and should never
contain any. Everything here is designed to read an installation you already
own, on your own machine, at runtime.
