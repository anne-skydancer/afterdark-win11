using System.Text.Json;
using System.Text.Json.Serialization;

namespace AfterDark.Studio.Models;

/// <summary>
/// One saved configuration of a module: the four control values, named.
///
/// The app owns its own store rather than writing After Dark's AFTERDRK.INI.
/// The four-slot INI cannot express presets, per-monitor choices, or several
/// saved configurations of the same module — and this way nothing we do can
/// corrupt the user's original installation.
/// </summary>
public sealed class Preset
{
    public string Name { get; set; } = "Default";
    public int[] Values { get; set; } = [0, 0, 0, 0];

    public Preset Clone() => new() { Name = Name, Values = (int[])Values.Clone() };
}

public sealed class ModuleSettings
{
    public string FileName { get; set; } = "";
    public List<Preset> Presets { get; set; } = [];
    public string ActivePreset { get; set; } = "Default";
}

public sealed class StudioSettings
{
    public string? InstallPath { get; set; }
    public string? SelectedModule { get; set; }
    public int TimeoutMinutes { get; set; } = 10;
    public bool SecureResume { get; set; }
    public int TargetFps { get; set; } = 30;
    public string ScalingMode { get; set; } = "Integer";
    public Dictionary<string, ModuleSettings> Modules { get; set; } = new(StringComparer.OrdinalIgnoreCase);

    [JsonIgnore]
    public static string DefaultPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "AfterDarkStudio", "settings.json");

    private static readonly JsonSerializerOptions Json = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    public static StudioSettings Load(string? path = null)
    {
        path ??= DefaultPath;
        try
        {
            if (File.Exists(path))
                return JsonSerializer.Deserialize<StudioSettings>(File.ReadAllText(path), Json)
                       ?? new StudioSettings();
        }
        catch (Exception ex) when (ex is IOException or JsonException or UnauthorizedAccessException)
        {
            // A corrupt or unreadable settings file must never stop the app.
        }
        return new StudioSettings();
    }

    public void Save(string? path = null)
    {
        path ??= DefaultPath;
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        // Write-then-replace so an interrupted save cannot truncate the file.
        var tmp = path + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(this, Json));
        File.Move(tmp, path, overwrite: true);
    }

    public ModuleSettings For(string fileName)
    {
        if (!Modules.TryGetValue(fileName, out var m))
            Modules[fileName] = m = new ModuleSettings { FileName = fileName };
        return m;
    }
}
