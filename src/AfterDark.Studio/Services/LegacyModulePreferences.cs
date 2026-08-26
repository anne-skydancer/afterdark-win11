using System.Runtime.InteropServices;
using System.Text;

namespace AfterDark.Studio.Services;

/// <summary>Compatibility preferences still consumed by original AD4 modules.</summary>
public static class LegacyModulePreferences
{
    private const string ArtCriticSection = "ArtCritic d29";
    private const string ArtCriticPathKey = "Art Path";

    public static string ModulesIniPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "VirtualStore", "Windows", "MODULES.INI");

    /// <summary>
    /// Point Art Critic at imported, user-owned images. Its original 32-bit
    /// module reads this key through Windows INI virtualization.
    /// </summary>
    public static bool EnsureArtCriticPath(string modulesPath, string? iniPath = null)
    {
        if (!OperatingSystem.IsWindows()) return false;
        if (!File.Exists(Path.Combine(modulesPath, "CRITIC.AD"))) return false;

        var pictures = Path.Combine(modulesPath, "PICTURES");
        if (!Directory.Exists(pictures)) return false;

        iniPath ??= ModulesIniPath;
        var directory = Path.GetDirectoryName(iniPath);
        if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);

        var existing = new StringBuilder(1024);
        GetPrivateProfileString(ArtCriticSection, ArtCriticPathKey, "",
                                existing, existing.Capacity, iniPath);
        if (Directory.Exists(existing.ToString())) return true;

        return WritePrivateProfileString(ArtCriticSection, ArtCriticPathKey,
                         Path.GetFullPath(pictures), iniPath);
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern uint GetPrivateProfileString(
        string section, string key, string defaultValue,
        StringBuilder returnedString, int size, string fileName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool WritePrivateProfileString(
        string section, string key, string value, string fileName);
}