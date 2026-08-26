using System.Text.Json;

namespace AfterDark.Catalog;

/// <summary>
/// Friendlier names for controls whose labels the module truncates.
///
/// szTitle is a fixed 14-byte field, so "Display Karaoke" is stored as
/// "Display Karaok". This is presentation only: it never changes a value, a
/// slot, or anything written back.
/// </summary>
public sealed class LabelOverrides
{
    private readonly Dictionary<string, string> _map = new(StringComparer.OrdinalIgnoreCase);

    public static LabelOverrides Empty { get; } = new();

    public static LabelOverrides Load(string path)
    {
        var result = new LabelOverrides();
        try
        {
            if (!File.Exists(path)) return result;
            using var doc = JsonDocument.Parse(File.ReadAllText(path));
            foreach (var p in doc.RootElement.EnumerateObject())
            {
                if (p.Name.StartsWith('_') || p.Value.ValueKind != JsonValueKind.String) continue;
                if (p.Value.GetString() is { Length: > 0 } v) result._map[p.Name] = v;
            }
        }
        catch (Exception ex) when (ex is IOException or JsonException)
        {
            // Cosmetic data: never let it break the catalogue.
        }
        return result;
    }

    public void Apply(AdModule module)
    {
        foreach (var c in module.Controls)
            if (_map.TryGetValue($"{module.FileName}/{c.Slot}", out var full))
                c.TitleOverride = full;
    }

    public void Apply(IEnumerable<AdModule> modules)
    {
        foreach (var m in modules) Apply(m);
    }
}
