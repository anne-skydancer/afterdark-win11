# Downloads

## [Get the installer from the Releases page](https://github.com/anne-skydancer/afterdark-win11/releases/latest)

**AfterDarkStudio-0.1.0-setup.exe** — 64-bit installer for Windows 10 1809 and
later. 32.2 MB, self-contained — no .NET runtime prerequisite.

Direct link once the release is published:
https://github.com/anne-skydancer/afterdark-win11/releases/download/v0.1.0/AfterDarkStudio-0.1.0-setup.exe

The installer is a **release asset, not repository content**. A 32 MB binary in
git history is paid for by every clone forever, and it is exactly the file that
misbehaves under partial and shallow clones. This folder holds the checksum and
the notes; the binary lives on the Releases page.

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

### If the release has not been published yet

Build it from source — the result is byte-identical given the same toolchain:

### Building it yourself

```
make dist        # everything into dist/
make installer   # Inno Setup 7
```

The installer in this folder was built from the commit it was committed in, with
Inno Setup 7.1.0. See [../docs/PACKAGING.md](../docs/PACKAGING.md).
