# Downloads

## AfterDarkStudio-0.1.5-setup.exe

64-bit installer for Windows 10 1809 and later. 32.1 MB, self-contained — no
.NET runtime prerequisite.

```
SHA-256  3700c98d69fb17b7d2d44b10b1c31306f2f35818102400f3b1ce9ad1bf3cc1b1
```

Verify before running:

```powershell
Get-FileHash .\AfterDarkStudio-0.1.5-setup.exe -Algorithm SHA256
```

### What changed

- Original module sound is enabled through the recovered `AD_MODULE32` bits
	`0x02 | 0x10`, matching the unmuted Berkeley Systems host.
- The x86 host recreates the legacy `[Berkeley Systems]` profile folder values
	consumed by ADXPL510, so relative media resolves below imported modules.
- Setup and in-app import preserve the five external AD4 MIDI tracks and rename
	the two Flying Toasters files to their embedded long filenames.

Real-media import produced 84 modules, 11 pictures, and five music tracks.
Flying Toasters with Music set to Always generated 80 MIDI file events against
the staged host: 75 successful operations and 52 reads from the imported track,
with no request under `C:\Windows\Music`. Bad Dog crash fallback remained
nonblank and exposed no WER dialog. Twelve managed tests and the native ABI
test pass.

The installer contains no Berkeley Systems content and is not code-signed, so
SmartScreen will warn.

## AfterDarkStudio-0.1.4-setup.exe

64-bit installer for Windows 10 1809 and later. 32.1 MB, self-contained — no
.NET runtime prerequisite.

```
SHA-256  d84c95e2a6e3663a656c3b10c93b64f73088fa1f99d9c2396b62fdf997277e85
```

Verify before running:

```powershell
Get-FileHash .\AfterDarkStudio-0.1.4-setup.exe -Algorithm SHA256
```

### What changed

- Fixed the blank error dialog produced when a selected legacy module crashes
	inside the isolated x86 renderer.
- Renderer and `.scr` processes suppress Windows crash UI and communicate
	failure through process exit.
- Preview and full-screen modes retry once with Flying Toasters, then Starry
	Night when available.
- All native binaries were freshly rebuilt for this release: x86 host and x64
	screensaver, alongside the x64 Studio shell.

The issue was reproduced with Bad Dog: it faults during `PREINITIALIZE` with
access violation `0xC0000005`. Final staged validation kept the `.scr` alive,
launched exactly one Flying Toasters fallback, rendered 105 sampled colors,
and exposed no Windows Error Reporting dialog. Twelve managed tests and the
native `AD_MODULE32` ABI layout test pass.

The installer contains no Berkeley Systems content and is not code-signed, so
SmartScreen will warn.

## AfterDarkStudio-0.1.3-setup.exe

64-bit installer for Windows 10 1809 and later. 32.2 MB, self-contained — no
.NET runtime prerequisite.

```
SHA-256  a02c7494cab99ddd00ecadc40073171cb6c3bb5a43cfc35f0cd5aec0e89c6181
```

Verify before running:

```powershell
Get-FileHash .\AfterDarkStudio-0.1.3-setup.exe -Algorithm SHA256
```

### What changed

- Setup and in-app import preserve Art Critic's user-owned BMP/GIF/JPEG files
	under `modules\PICTURES`.
- Studio seeds the original module's discovered `[ArtCritic d29] / Art Path`
	preference and preserves an existing valid custom folder.
- Art Critic remains the original, unmodified 32-bit `CRITIC.AD`; no Berkeley
	Systems module or image is included in this download.

Real-media import was verified against the mounted Deluxe ISO: 84 modules and
11 Art Critic pictures copied, with no unsupported files. Twelve automated
tests pass. The minimal recovered host still produces a blank Art Critic
surface despite reading the seeded INI; this is tracked as a host run-mode
compatibility issue rather than a reason to rewrite an already-PE32 module.

The installer is not code-signed, so SmartScreen will warn.

## AfterDarkStudio-0.1.2-setup.exe

64-bit installer for Windows 10 1809 and later. 32.2 MB, self-contained — no
.NET runtime prerequisite.

```
SHA-256  765784cc3dc29e2b09795e9002417cbac9e320430f08f72c0e87d800a8574eeb
```

Verify before running:

```powershell
Get-FileHash .\AfterDarkStudio-0.1.2-setup.exe -Algorithm SHA256
```

### What changed

- Closing Studio keeps it available under an `AD` monogram in the Windows
	notification area.
- The tray menu switches directly between all 23 runnable modules, opens the
	full Studio, opens Windows screensaver settings, and exits explicitly.
- Module switching no longer writes timeout or secure-resume preferences;
	those remain exclusively controlled by Windows.
- The Classic rewrite study now incorporates direct evidence from all 61 NE
	modules and recommends Mandelbrot as the first native PE32 rewrite pilot.

The staged release was exercised natively on Windows 11: close-to-tray and
restore work, the native menu exposes all runnable modules, Flying Toasters was
activated through the tray, and registry snapshots proved timeout and secure
resume remained unchanged. All 11 automated tests pass.

The installer contains no Berkeley Systems content and is not code-signed, so
SmartScreen will warn.

## AfterDarkStudio-0.1.1-setup.exe

64-bit installer for Windows 10 1809 and later. 32.2 MB, self-contained — no
.NET runtime prerequisite.

```
SHA-256  9c126fcfca88fbba1339fdd662397205cef2663a384cb831bc5c885b49177f2c
```

Verify before running:

```powershell
Get-FileHash .\AfterDarkStudio-0.1.1-setup.exe -Algorithm SHA256
```

### What changed

- Studio follows the Windows app theme and system accent color.
- Timeout and secure-resume controls reflect the live Windows session settings.
- Normal startup loads the imported catalogue instead of opening an unbound UI.
- Modules can be imported from owned media inside Studio as well as during Setup.
- An empty-library state guides first-time import when Setup was skipped.

This build was compiled natively on Windows 11 with Inno Setup 7. The Studio UI
was exercised against the mounted 84-module After Dark Deluxe disc, and all 11
automated tests pass. The installer is not code-signed, so SmartScreen will
warn.

The installer contains no Berkeley Systems content. During Setup, select your
mounted After Dark disc or an existing installation; modules are copied into
the application directory and removed by the uninstaller.

## AfterDarkStudio-0.1.0-setup.exe

64-bit installer for Windows 10 1809 and later. 32.2 MB, self-contained — no
.NET runtime prerequisite.

```
SHA-256  2c57021bbbee0ad7e1edd2e060ce999bb975b873484de00c27c3f2d5cdabfb58
```

Verify before running:

```powershell
Get-FileHash .\AfterDarkStudio-0.1.0-setup.exe -Algorithm SHA256
```

### Read this first

**This build has never been run on Windows 11.** It was developed and verified
on Linux: the modules, the After Dark engine, the Win32 API and the Inno Setup
compiler are all real, but they were exercised through Wine. The calling
convention, parameter block, message numbering, module lifecycle, palette
handling, screensaver path and installer compilation are all confirmed; running
on the actual target is not. Treat 0.1.0 as a first cut and expect rough edges.

**It is not code-signed.** SmartScreen will warn. The SHA-256 above is the only
integrity check on offer.

### What you need

A licensed copy of After Dark 4 — the disc, or an existing installation. This
installer contains **no** Berkeley Systems code, artwork or sound, and never
will. Setup asks where your copy is and imports the screen savers from it.

That import is the point: After Dark 4's own installer is 16-bit and cannot run
on Windows 11 at all, so the disc is otherwise uninstallable. Point Setup at it
and the modules are copied in.

### What runs

Checked against all 84 modules on the AD4 Deluxe disc:

| Set | Count | Runs | Settings readable |
|---|---:|---|---|
| `AD40/` | 22 | yes | yes |
| `ENGINE/STARRYNI.AD` (Starry Night) | 1 | yes | yes |
| `CLASSIC/` (the After Dark 3-era library) | 61 | **no — 16-bit** | yes |

The Classic set is 16-bit, and no 64-bit Windows can load 16-bit code in any
process. After Dark 3 is 16-bit throughout — its engine, `ADXPL300.DLL`, is
itself an NE binary — so the same applies to the whole AD3 library. Those
modules are still catalogued and their settings still readable, which is what
makes a faithful rewrite possible. See [../docs/REWRITES.md](../docs/REWRITES.md).

### Installing

Setup elevates and installs system-wide into Program Files for all users.
Optional tasks copy the `.scr` into System32 so it lists in every user's Screen
Saver dropdown, and seed a machine-wide default configuration.

Choosing a screen saver stays per-user: Windows makes that a User Configuration
setting on `HKCU\Control Panel\Desktop`, with no machine-wide equivalent. Open
After Dark Studio and press **Set as screensaver**.

### Uninstalling

Removes the program and the imported modules, and clears `SCRNSAVE.EXE` if it
still points at this screen saver. Per-user settings are left alone.

### Building it yourself

```
make dist        # everything into dist/
make installer   # Inno Setup 7
```

The installer in this folder was built from the commit it was committed in, with
Inno Setup 7.1.0. See [../docs/PACKAGING.md](../docs/PACKAGING.md).
