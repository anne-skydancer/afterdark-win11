# Changelog

## 0.1.0 — first release

A modern Windows 11 front end for After Dark 4, replacing the
copy-to-System32-and-import-a-registry-blob ritual with an application.

### Added

- **Module ABI, recovered by disassembly.** After Dark 4 uses
  `int __stdcall Module(AD_MODULE32*)` — one argument, not AD3's three —
  resolved as `_Module@4` with an undecorated `Module` fallback. The parameter
  block is 348 bytes with its own size in the first DWORD. Messages were
  renumbered from AD3 (`BLANK=3`, `DRAWFRAME=4`, `CLOSE=5`); a host built to the
  published AD3 constants would silently send the wrong ones. See
  [docs/ABI.md](docs/ABI.md) and [include/ad_module32.h](include/ad_module32.h).
- **`admhost32`** — 32-bit host that loads the engine and a module and renders
  frames. Must be 32-bit: a 64-bit process cannot load a 32-bit DLL.
- **After Dark Studio** — x64 catalogue and settings UI. Each module's controls
  are read out of its own binary, so configuration needs nothing from the dead
  Windows 95 control panel. Live preview streams real frames from the host.
- **`AfterDarkModern.scr`** — x64 screensaver front end: `/s` `/p` `/c`, one
  full-screen window per monitor, frame pacing, integer scaling.
- **Registration without hacks** — three values under `HKCU\Control Panel\Desktop`
  plus `SystemParametersInfo`. `SCRNSAVE.EXE` takes a full path, so no System32
  copy and no `.reg` import are required.
- **Inno Setup 7 installer** — system-wide, `SetupArchitecture=x64`, and it
  imports your screen savers from your own disc or installation. That replaces
  After Dark 4's own installer, which is 16-bit and cannot run on Windows 11.
- **Tooling** — `ad_inventory.py` and `ad_extract.py` report what runs and dump
  every module's configuration schema. Read-only; they never execute a module.

### Known limitations

- **Never run on Windows 11.** Verified through Wine on Linux against real
  modules, the real engine and the real Inno compiler.
- **Not code-signed.**
- **16-bit modules cannot run** — 61 of the 84 on the AD4 disc, and the whole
  After Dark 3 library. This is a Windows limitation, not a gap in this software.
- Module `CTL_BUTTON` controls are catalogued but not yet wired to
  `BUTTONMESSAGE`, so a module's own dialogs are not reachable from the UI.
