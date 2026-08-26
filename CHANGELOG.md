# Changelog

## 0.1.6 — native Windows Studio

### Changed

- Studio now uses WPF on .NET 10 with the built-in system theme and Windows
  accent resources, replacing the cross-platform Avalonia presentation layer.
- Tray controls use the native Windows notification-area implementation, while
  folder import, streamed module preview, settings, and screenshot workflows
  retain their existing behavior.
- The former Avalonia 11.3.20 Studio remains independently buildable under
  `archive/AfterDark.Studio.Avalonia` for historical comparison.

### Verified

- The WPF shell enumerates all 84 modules from owned media and captures live
  Flying Toasters pixels through the x86 host frame stream.
- All 12 managed tests pass on .NET 10, self-contained `win-x64` publishing
  succeeds, and the active dependency graph has no known vulnerabilities.

## 0.1.5 — original module sound

### Fixed

- The recovered host now sets `AD_MODULE32` sound bits `0x02 | 0x10`, matching
  the original host whenever `MuteSound` was false. ADXPL510 checks both bits
  before enabling its sound path.
- The host initializes the original `[Berkeley Systems]` profile folder values
  before loading the engine, so relative media paths resolve below the imported
  module directory instead of `C:\Windows`.
- Setup and in-app import preserve all five external AD4 MIDI tracks, including
  the long `Baby Toasters.mid` and `Flying Toasters.mid` names embedded by
  Flying Toasters.

### Verified

- Real Deluxe media import produced 84 modules, 11 Art Critic pictures, and all
  five expected music tracks.
- Flying Toasters with Music set to Always produced 79 MIDI file events,
  including 74 successful operations and 52 reads of the imported track, with
  zero requests under `C:\Windows\Music`.

## 0.1.4 — screensaver crash recovery

### Fixed

- A module crash in Windows preview or full-screen mode no longer produces a
  blank Windows Error Reporting dialog. The isolated x86 host and x64 `.scr`
  suppress child crash UI and report failure through process exit.
- The `.scr` retries once with Flying Toasters, then Starry Night when
  available, so one incompatible module cannot leave the preview or lock
  screen blank.
- Portable/staged `.scr` copies prefer sibling host binaries, while a System32
  copy continues to resolve the Program Files installation through HKLM.
- Hosted modules now receive a hidden HWND matching their 640x480 render
  surface rather than the differently sized preview/full-screen presentation
  target.

### Verified

- Bad Dog reproducibly faults during `PREINITIALIZE` with access violation
  `0xC0000005` in hosted mode. Under the fixed `.scr`, Windows preview remains
  alive, switches to Flying Toasters, renders a 76-color sampled surface, and
  shows no WER dialog.
- Flying Toasters still renders nonblank frames through the rebuilt x86 host,
  and the `/c` configuration contract continues to launch Studio.

## 0.1.3 — Art Critic companion media

### Added

- **Art Critic picture import.** Setup and in-app import copy BMP, GIF, JPG, and
  JPEG files from the owned disc's `AD40\PICTURES` folder into
  `modules\PICTURES`.
- **Legacy preference bridge.** Studio seeds Art Critic's discovered
  `[ArtCritic d29] / Art Path` compatibility setting through Windows' INI API,
  while preserving any valid custom folder already selected by the user.

### Clarified

- Art Critic itself is already a hostable 32-bit PE module using the recovered
  `AD_MODULE32` ABI. It remains unmodified; only its external user-owned media
  and preference path need restoration.
- Native ABI rewrites remain the recommended path for the 61 Classic NE
  modules, with Mandelbrot as the first procedural pilot.

## 0.1.2 — tray controls and Windows-owned timeout

### Added

- **Tray module switcher.** Closing Studio keeps it in the Windows notification
  area. Its native menu lists every runnable module for one-click activation,
  opens the full Studio, links to Windows screensaver settings, and provides an
  explicit Exit command.
- **AD monogram tray icon.** A small, high-contrast amber-and-white monogram is
  generated at runtime for clear rendering on light and dark taskbars.
- **Measured Classic rewrite plan.** All 61 NE modules, supporting binaries,
  controls, and representative resources were assessed. Mandelbrot replaces
  Confetti as the first native rewrite pilot.

### Changed

- Screensaver timeout and secure resume are now exclusively owned by Windows.
  Selecting a module changes only the active `.scr`; Studio no longer stores or
  writes duplicate timing/security preferences.
- Restoring Studio from the tray restarts its live preview, and importing media
  refreshes the tray module list without restarting the app.

## 0.1.1 — Windows integration and media import

### Added

- **In-app media import.** Studio can import modules from an owned After Dark
  disc or existing installation into the current user's local data directory.
  AD4 and Classic modules remain separated to preserve duplicate filenames.
- **First-run empty state.** A clear import action replaces the blank window
  shown when Setup did not import modules.

### Changed

- Studio follows the Windows app theme and system accent color instead of
  forcing a custom dark palette.
- Screensaver timeout and secure-resume controls initialize from the live
  Windows session settings.
- Normal application startup now loads persisted settings and the imported
  module catalogue before displaying the main window.

### Verified

- The Studio UI and mounted-disc catalogue were exercised natively on Windows
  11 with both Windows personalization and the real 84-module disc layout.
- Media import has automated coverage for AD4/Classic separation, source
  immutability, and invalid source handling.

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
