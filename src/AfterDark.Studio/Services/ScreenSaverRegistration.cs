using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace AfterDark.Studio.Services;

/// <summary>
/// Registers a .scr as the active screensaver.
///
/// This is what replaces the ritual of copying a .scr into System32 and
/// importing a .reg file. Neither is necessary:
///
///   * SCRNSAVE.EXE accepts a FULL PATH. The System32 copy only ever existed
///     so the classic dialog would list the file in its dropdown.
///   * These are three values under HKEY_CURRENT_USER. No elevation, no
///     writes to a system directory, no opaque .reg from the internet.
///
/// HKCU\Control Panel\Desktop is not subject to WOW64 registry redirection,
/// so a 64-bit process writes exactly the same keys a 32-bit one would.
///
/// NOTE ON SYSTEM-WIDE INSTALLS. Which screensaver is active is a per-user
/// setting by Windows design: the Group Policy that controls it lives under
/// User Configuration and operates on HKEY_CURRENT_USER\Control Panel\Desktop.
/// There is no HKLM equivalent the shell honours. A machine-wide install
/// therefore puts the binaries where every user can run them and seeds a
/// machine-wide default configuration, but each user still activates the
/// screensaver for their own session -- here, or from the Windows dialog.
/// </summary>
[SupportedOSPlatform("windows")]
public static class ScreenSaverRegistration
{
    private const string DesktopKey = @"Control Panel\Desktop";

    public sealed record State(string? ScreenSaverPath, bool Active, int TimeoutSeconds, bool Secure);

    public static State Read()
    {
        using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(DesktopKey);
        if (key is null) return new State(null, false, 0, false);
        return new State(
            key.GetValue("SCRNSAVE.EXE") as string,
            (key.GetValue("ScreenSaveActive") as string) == "1",
            int.TryParse(key.GetValue("ScreenSaveTimeOut") as string, out var t) ? t : 0,
            (key.GetValue("ScreenSaverIsSecure") as string) == "1");
    }

    /// <summary>Point Windows at <paramref name="scrPath"/> and apply it live.</summary>
    public static void Install(string scrPath, int timeoutSeconds, bool secureResume)
    {
        if (!File.Exists(scrPath))
            throw new FileNotFoundException("screensaver not found", scrPath);
        if (timeoutSeconds < 60) timeoutSeconds = 60;

        using var key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(DesktopKey)
            ?? throw new InvalidOperationException("could not open HKCU Control Panel\\Desktop");

        key.SetValue("SCRNSAVE.EXE", Path.GetFullPath(scrPath));
        key.SetValue("ScreenSaveActive", "1");
        key.SetValue("ScreenSaveTimeOut", timeoutSeconds.ToString());
        key.SetValue("ScreenSaverIsSecure", secureResume ? "1" : "0");

        Apply(timeoutSeconds, secureResume);
    }

    public static void Disable()
    {
        using var key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(DesktopKey);
        key?.SetValue("ScreenSaveActive", "0");
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, IntPtr.Zero, SPIF_SENDCHANGE);
    }

    /// <summary>
    /// Push the values into the running session. Without this the registry is
    /// right but the current session keeps the old timeout until sign-out.
    /// </summary>
    private static void Apply(int timeoutSeconds, bool secureResume)
    {
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, IntPtr.Zero, SPIF_SENDCHANGE);
        SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, (uint)timeoutSeconds, IntPtr.Zero, SPIF_SENDCHANGE);
        SystemParametersInfo(SPI_SETSCREENSAVESECURE, secureResume ? 1u : 0u, IntPtr.Zero, SPIF_SENDCHANGE);
    }

    private const uint SPI_SETSCREENSAVEACTIVE  = 0x0011;
    private const uint SPI_SETSCREENSAVETIMEOUT = 0x000F;
    private const uint SPI_SETSCREENSAVESECURE  = 0x0077;
    private const uint SPIF_SENDCHANGE          = 0x0002;

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SystemParametersInfo(uint action, uint param, IntPtr pv, uint winIni);
}
