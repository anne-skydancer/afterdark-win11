using System.Runtime.Versioning;

namespace AfterDark.Studio.Services;

/// <summary>
/// Where the program's own files live, and where settings go.
///
/// The install directory is recorded in HKLM by Setup rather than inferred from
/// where a binary happens to sit: a system-wide install may copy the .scr into
/// System32 so it shows up in every user's Screen Saver dropdown, while
/// admhost32.exe and the shell stay in Program Files.
/// </summary>
public static class AppPaths
{
    public const string RegistryKey = @"SOFTWARE\AfterDarkStudio";
    public const string ScreenSaverFileName = "AfterDarkModern.scr";
    public const string StudioFileName = "AfterDark.Studio.exe";
    public const string HostFileName = "admhost32.exe";

    /// <summary>Setup's recorded install directory, or where this assembly sits.</summary>
    public static string InstallDir
    {
        get
        {
            if (OperatingSystem.IsWindows() && ReadInstallDir() is { Length: > 0 } dir
                && Directory.Exists(dir))
                return dir;
            return AppContext.BaseDirectory;
        }
    }

    [SupportedOSPlatform("windows")]
    private static string? ReadInstallDir()
    {
        foreach (var root in new[]
                 {
                     Microsoft.Win32.Registry.LocalMachine,
                     Microsoft.Win32.Registry.CurrentUser,
                 })
        {
            try
            {
                using var key = root.OpenSubKey(RegistryKey);
                if (key?.GetValue("InstallDir") is string s && s.Length > 0) return s;
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                // A missing or unreadable key just means "not installed that way".
            }
        }
        return null;
    }

    public static string ScreenSaver => Path.Combine(InstallDir, ScreenSaverFileName);
    public static string Studio      => Path.Combine(InstallDir, StudioFileName);
    public static string Host        => Path.Combine(InstallDir, HostFileName);

    /// <summary>This user's settings. Always writable.</summary>
    public static string UserDataDir => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "AfterDarkStudio");

    /// <summary>
    /// The machine-wide defaults every user falls back to. Readable by all,
    /// writable only by an administrator — which is exactly the intent.
    /// </summary>
    public static string MachineDataDir => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
        "AfterDarkStudio");

    public static string UserSaverConfig    => Path.Combine(UserDataDir, "saver.cfg");
    public static string MachineSaverConfig => Path.Combine(MachineDataDir, "saver.cfg");

    /// <summary>True if this process could write the machine-wide defaults.</summary>
    public static bool CanWriteMachineDefaults()
    {
        try
        {
            Directory.CreateDirectory(MachineDataDir);
            var probe = Path.Combine(MachineDataDir, ".probe");
            File.WriteAllText(probe, "");
            File.Delete(probe);
            return true;
        }
        catch (Exception ex) when (ex is UnauthorizedAccessException or IOException)
        {
            return false;
        }
    }
}
