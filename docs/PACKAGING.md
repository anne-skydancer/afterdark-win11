# Packaging, architecture, and the installer

How the pieces fit, why exactly one of them is 32-bit, and how it ships.

---

## 1. What is 64-bit, and what cannot be

The goal was a 64-bit application. Everything is 64-bit **except the one
process that touches a module**, and that exception is not a choice:

| Component | Arch | Why |
|---|---|---|
| `AfterDark.Studio.exe` | **x64** | The shell: catalogue, settings, registration. Reads module files as *data*; never loads one. |
| `AfterDarkModern.scr` | **x64** | Handles `/s` `/p` `/c`, owns the full-screen windows and input. Spawns hosts; loads nothing itself. |
| `admhost32.exe` | **x86** | Loads `ADXPL510.DLL` and the `.AD`. **A 64-bit process cannot load a 32-bit DLL.** No flag, manifest or shim changes this. |

Verified in the built output:

```
AfterDark.Studio.exe   PE32+ executable (GUI) x86-64
AfterDarkModern.scr    PE32+ executable (GUI) x86-64
admhost32.exe          PE32  executable (console) Intel 80386
```

The split costs nothing and buys crash isolation: a 1996 module that faults
takes down a child process, not the UI and not the lock screen.

**HWNDs cross the boundary safely.** A window handle is a 32-bit value even in
64-bit processes, so the x64 `.scr` creates a window per monitor and passes its
handle to an x86 child, which renders into it as a child window. That is how
`/p` preview works too — the settings dialog's preview HWND is forwarded
straight through.

The catalogue is fully 64-bit because it never executes anything: it parses PE
and NE headers, resources and control definitions as data. That is also why the
gallery can describe 16-bit modules it can never run.

---

## 2. Process flow

```
Windows idle timer
        │
        ▼
AfterDarkModern.scr /s            (x64)
        │  reads %LOCALAPPDATA%\AfterDarkStudio\saver.cfg
        │  EnumDisplayMonitors -> one full-screen window each
        │
        ├── admhost32.exe --parent <hwnd> …   (x86)  monitor 1
        └── admhost32.exe --parent <hwnd> …   (x86)  monitor 2
                 │
                 └── LoadLibrary ADXPL510.DLL + FOO.AD
                     Module(PREINITIALIZE) → BLANK → DRAWFRAME…

any key / mouse movement → .scr closes children, exits
```

`saver.cfg` is a small `key=value` file rather than JSON on purpose: the `.scr`
must start instantly, so it carries no parser and no runtime. Studio owns the
real settings store and projects the active choice down into it.

---

## 3. System-wide install: what that can and cannot mean

The program installs **once, for every user**: Program Files, elevated, one
copy. That part is straightforward.

Which screensaver is *active* is a different matter, and Windows decides it:
the screensaver Group Policy lives under **User Configuration** and operates on
`HKEY_CURRENT_USER\Control Panel\Desktop`. There is no HKLM equivalent the shell
honours. Microsoft's own guidance on the policy not applying points at the same
per-user key.

So "system-wide" is delivered in the three ways that actually exist:

| Goal | Mechanism |
|---|---|
| One installed copy for all users | Program Files, `PrivilegesRequired=admin` |
| Every user gets a working configuration | machine-wide default in `%ProgramData%\AfterDarkStudio\saver.cfg`, overridden by the user's own |
| It appears in every user's Screen Saver list | optional copy of the `.scr` into `System32` |
| Enforced across a fleet | Group Policy — User Configuration → Administrative Templates → Control Panel → Personalization, or Group Policy Preferences writing `HKCU\Control Panel\Desktop`. Not the installer's job. |

Turning the screensaver *on* stays each user's choice. An installer that
switches it on for everyone is the behaviour this project replaces.

### The path trap this creates

With the System32 option, the `.scr` no longer sits beside `admhost32.exe`.
Anything resolving siblings relative to the running binary breaks the moment
Windows launches the System32 copy.

Setup therefore records:

```
HKLM\SOFTWARE\AfterDarkStudio\InstallDir = C:\Program Files\After Dark Studio
```

and both the `.scr` and Studio resolve program files through it, falling back to
"beside me" only for a portable unpacked copy. Verified by running the `.scr`
from a directory containing nothing else, with the host elsewhere: it found the
host and rendered.

### Config resolution

```
%LOCALAPPDATA%\AfterDarkStudio\saver.cfg      this user's choice   (wins)
%ProgramData%\AfterDarkStudio\saver.cfg       machine-wide default (fallback)
```

The ProgramData default is readable by everyone and writable only by
administrators, which is the default ACL and exactly the intent. Studio offers
"Default for all users", enabled only when it can actually write there.

---

## 4. The installer

`installer/AfterDarkStudio.iss`, built with **Inno Setup 7** (7.0+; 7.1.0 was
current when written). Either compiler edition can build it — the output is a
native 64-bit installer.

```ini
SetupArchitecture=x64
```

That one Inno 7 directive is the whole 64-bit story: with it set,
`ArchitecturesAllowed` and `ArchitecturesInstallIn64BitMode` both default to
`x64compatible`, so no further architecture directives are needed.

### Deliberate omissions

The installer exists to replace a ritual, so it does none of the ritual:

- **No screensaver registry keys are written at install time.** Choosing a
  screensaver is the user's decision, made in the app, per user, at HKCU.
  An installer that silently commandeers the screensaver is precisely the
  behaviour this project replaces.
- **The System32 copy is optional and off nothing.** It is a task the user can
  decline; registration works by full path either way. Its only purpose is
  making the entry visible in the Windows dropdown.
- **No Berkeley Systems content.** The installer ships no engine, module,
  artwork or sound. `LICENSE-NOTE.txt` says so, and it is the license page.

### Tasks

| Task | Effect |
|---|---|
| `systemscr` | copy the `.scr` into `System32` so it lists for every user |
| `machinedefault` | look for an After Dark installation and seed `%ProgramData%` with a working default |
| `desktopicon` | common desktop shortcut |

### Uninstall

If `SCRNSAVE.EXE` still points at either of our `.scr` locations, uninstall
clears it and sets `ScreenSaveActive=0`. Leaving a dangling path gives a
screensaver that silently does nothing and a settings dialog that looks broken.

This can only fix the account running the uninstaller — the setting lives in
each user's own hive, and an uninstaller has no business walking other people's
profiles. Another user whose screensaver was ours simply picks a new one, which
is visible and recoverable. The machine-wide default under `%ProgramData%` is
removed; per-user settings are left alone, because deleting someone's
preferences out of their profile is not an uninstaller's decision.

### .NET

Studio publishes **self-contained win-x64**, so the installer needs no .NET
runtime prerequisite. That costs roughly 90 MB in `dist/`. For a smaller
download, drop `--self-contained` and add a .NET 8 Desktop Runtime check to
`[Code]`; the trade is a prerequisite the user may have to install.

---

## 4. Registration, in full

Three values, `HKEY_CURRENT_USER\Control Panel\Desktop`:

```
SCRNSAVE.EXE        = C:\...\AfterDarkModern.scr     (full path)
ScreenSaveActive    = "1"
ScreenSaveTimeOut   = "600"                          (seconds)
ScreenSaverIsSecure = "0" | "1"
```

then `SystemParametersInfo` with `SPIF_SENDCHANGE` so the running session picks
it up — without that the registry is right but the current session keeps the
old timeout until sign-out.

`HKCU\Control Panel\Desktop` is not subject to WOW64 registry redirection, so
the 64-bit shell writes exactly the keys a 32-bit one would. Secure resume is
delegated to Windows via `ScreenSaverIsSecure`; AD4's `PASSWORD.CPL` call has no
modern equivalent and needs none.

---

## 5. Building

```
make dist        # everything into dist/
make test        # ABI layout check + catalogue tests
make installer   # Inno Setup 7
```

The `.iss` has been compiled with the real Inno Setup 7.1.0 compiler; it
produces `AfterDarkStudio-0.1.0-setup.exe`, a genuine x86-64 binary (machine
type `0x8664`), which is `SetupArchitecture=x64` doing its job.

Cross-building from Linux needs `mingw-w64` (i686 **and** x86_64) and the .NET 8
SDK. On Windows, use MSVC for the two native pieces and `dotnet publish` for the
shell; the `dist/` layout is identical either way.
