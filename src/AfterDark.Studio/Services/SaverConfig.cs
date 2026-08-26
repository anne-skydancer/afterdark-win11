using System.Globalization;
using System.Text;
using AfterDark.Studio.Models;

namespace AfterDark.Studio.Services;

/// <summary>
/// Writes the small key=value file the .scr reads at launch.
///
/// The .scr deliberately has no JSON parser: it must start instantly and do as
/// little as possible before pixels appear. Studio owns the real settings
/// store and projects the active choice down into this file.
/// </summary>
public static class SaverConfig
{
    public static string DefaultPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "AfterDarkStudio", "saver.cfg");

    public static void Write(
        string installPath,
        string modulePath,
        int[] controls,
        StudioSettings settings,
        string? studioExe = null,
        int width = 640,
        int height = 480,
        int bpp = 8,
        string? path = null)
    {
        path ??= DefaultPath;
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        var sb = new StringBuilder();
        sb.AppendLine("# Written by After Dark Studio. Edited here, overwritten there.");
        sb.AppendLine($"install={installPath}");
        sb.AppendLine($"module={modulePath}");
        if (!string.IsNullOrEmpty(studioExe)) sb.AppendLine($"studio={studioExe}");
        sb.AppendLine($"controls={string.Join(',', controls.Select(v => v.ToString(CultureInfo.InvariantCulture)))}");
        sb.AppendLine($"fps={settings.TargetFps}");
        sb.AppendLine($"scale={(settings.ScalingMode.Equals("Stretch", StringComparison.OrdinalIgnoreCase) ? "stretch" : "integer")}");
        sb.AppendLine($"bpp={bpp}");
        sb.AppendLine($"width={width}");
        sb.AppendLine($"height={height}");

        var tmp = path + ".tmp";
        File.WriteAllText(tmp, sb.ToString());
        File.Move(tmp, path, overwrite: true);
    }
}
