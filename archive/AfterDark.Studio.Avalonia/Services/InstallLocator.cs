using System.Runtime.Versioning;

namespace AfterDark.Studio.Services;

/// <summary>
/// Finds an After Dark installation. The app never ships engine or module
/// files; it works against whatever the user already has.
/// </summary>
public static class InstallLocator
{
    public const string EngineDll = "ADXPL510.DLL";

    /// <summary>A folder is usable only if the engine is there — modules import from it.</summary>
    public static bool IsInstall(string folder) =>
        Directory.Exists(folder) && File.Exists(Path.Combine(folder, EngineDll));

    [SupportedOSPlatform("windows")]
    public static IEnumerable<string> Candidates()
    {
        foreach (var special in new[]
                 {
                     Environment.SpecialFolder.ProgramFilesX86,
                     Environment.SpecialFolder.ProgramFiles,
                 })
        {
            var root = Environment.GetFolderPath(special);
            if (string.IsNullOrEmpty(root)) continue;
            foreach (var name in new[] { "After Dark", "Berkeley Systems\\After Dark", "AFTERDRK" })
            {
                var p = Path.Combine(root, name);
                if (IsInstall(p)) yield return p;
                // AD4 keeps modules in AD40/ and CLASSIC/ under FILES/
                var files = Path.Combine(p, "FILES", "AD40");
                if (IsInstall(files)) yield return files;
            }
        }

        foreach (var drive in new[] { "C", "D" })
        {
            var p = $@"{drive}:\AFTERDRK";
            if (IsInstall(p)) yield return p;
        }
    }

    [SupportedOSPlatform("windows")]
    public static string? FindFirst() => Candidates().FirstOrDefault();
}
