# Native Windows UI evaluation

## Implemented decision

After Dark Studio uses **WPF on .NET 10**. The previous Avalonia 11.3.20 shell
is preserved, buildable but inactive, in `archive/AfterDark.Studio.Avalonia`.

WPF is the best fit for this application even though WinUI 3 is Microsoft's
newest native UI framework. Studio is a Windows-only tray utility with a dense
settings window, streamed bitmap preview, existing XAML/MVVM structure, and a
custom unpackaged Inno installer. WPF preserves those strengths while removing
the cross-platform runtime and Linux dependencies.

Do not rewrite the Studio shell as raw Win32. Do not adopt WinUI 3 for this
migration.

## Options

| Option | Fit | Main advantage | Main cost |
|---|---|---|---|
| WPF, .NET 10 | **Recommended** | Windows-only Microsoft stack; direct port of XAML/MVVM; simple unpackaged deployment | Fluent styling is still evolving; tray uses WinForms `NotifyIcon` or `Shell_NotifyIcon` |
| WinUI 3 | Possible, not recommended | Best first-party Windows 11 control styling | Windows App SDK bootstrap/runtime deployment; no simpler tray story; more HWND/picker interop |
| Raw Win32/C++ | Poor | Smallest truly native dependency surface | Rebuilds binding, templating, accessibility, DPI, controls, and the entire managed catalogue UI |
| Keep Avalonia | Lowest immediate risk | Already working and tested | Cross-platform dependencies bring no product value for a Windows-only screensaver manager |

## Why WPF fits

- .NET 10 WPF includes the Windows 11 Fluent theme, system light/dark mode, and
  live Windows accent-color resources.
- `System.Windows.Forms.NotifyIcon` provides the exact persistent tray icon and
  native context-menu behavior Studio needs.
- `Microsoft.Win32.OpenFolderDialog` replaces Avalonia's storage provider.
- `WriteableBitmap.WritePixels` accepts the existing BGRA frame stream without
  changing `ModulePreview` or the x86 host protocol.
- `RenderTargetBitmap` can preserve the development screenshot command on a
  Windows desktop session.
- Self-contained WPF publishing works with the existing unpackaged Inno Setup
  flow. It needs no Windows App SDK bootstrapper or MSIX runtime deployment.
- The x64 `.scr`, x86 host, catalogue, importer, settings store, registration,
  and rewrite modules are UI-framework independent.

## Why not WinUI 3

WinUI 3 has the strongest native Windows 11 visual language, but it adds the
Windows App SDK deployment layer. An unpackaged app must either bootstrap a
machine-installed runtime or carry the Windows App SDK as self-contained
files. A fully self-contained .NET WinUI app cannot be a single executable.

Studio would still need explicit tray integration through `Shell_NotifyIcon`
or WinForms interop. Desktop file pickers also require HWND initialization,
and window hide/restore lifetime is more involved than WPF's established
desktop application model. Those costs do not buy meaningful capability for
this app.

## Implemented surface

The application/window/program Avalonia shell was replaced. The models,
services, module hosting, settings, registration, and view-model logic remain
shared in the active project.

The sole view-model presentation leak was replaced:

```text
ModuleViewModel.BadgeBrush -> ModuleViewModel.BadgeColor
```

The WPF layer then maps that state to `SystemColors`/theme brushes.

| Current surface | WPF replacement |
|---|---|
| `App.axaml` / `App.axaml.cs` | `App.xaml`; `ThemeMode="System"`; `NotifyIcon` lifetime |
| `MainWindow.axaml` | WPF `MainWindow.xaml` with equivalent templates and triggers |
| Avalonia `WriteableBitmap` | WPF `WriteableBitmap.WritePixels` on `Dispatcher` |
| `StorageProvider.OpenFolderPickerAsync` | `Microsoft.Win32.OpenFolderDialog` |
| Avalonia `TrayIcon` / `NativeMenu` | WinForms `NotifyIcon` / `ContextMenuStrip` |
| Avalonia headless screenshot | WPF `RenderTargetBitmap` on Windows |
| Avalonia theme resources | WPF Fluent theme and `SystemColors.AccentColorBrushKey` |

## Deployment measurement

Measured on this machine with .NET SDK 10.0.400, self-contained `win-x64`:

| Payload | Files | Size |
|---|---:|---:|
| Current Studio, Avalonia 11.3.20 / .NET 10 | 223 | 101.9 MiB |
| Blank WPF / .NET 10 application | 244 | 131.1 MiB |

WPF therefore is not expected to reduce the installer size. Trimming could be
evaluated later, but should not be mixed into the UI migration because XAML,
reflection, and tray behavior need separate validation.

## Validation state

- The active project targets `net10.0-windows` with WPF and WinForms tray
  integration enabled.
- System theme/accent, module templates, BGRA preview, folder import, tray
  lifetime, and screenshot mode are ported.
- The existing 12 managed tests pass against the Windows target.
- A real-media smoke run enumerates all 84 modules and captures live preview
  pixels through WPF.
- Installer publication remains deliberately deferred until operator tray and
  full-screen screensaver parity checks are complete.