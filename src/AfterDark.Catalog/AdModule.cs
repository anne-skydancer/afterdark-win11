using System.Text;

namespace AfterDark.Catalog;

public enum Generation { Ad4, Classic }

/// <summary>One module in the catalogue, described entirely from its binary.</summary>
public sealed class AdModule
{
    public required string Path { get; init; }
    public required string FileName { get; init; }
    public required string Title { get; init; }
    public string Credits { get; init; } = "";
    public required Generation Generation { get; init; }

    /// <summary>"AD40" or "Classic" — how a duplicate title is qualified.</summary>
    public string GenerationLabel => Generation == Generation.Ad4 ? "AD40" : "Classic";

    /// <summary>
    /// Set when another module in the same catalogue shares this title. The AD4
    /// disc ships RAIN.AD in both the AD40 and Classic sets, and two entries
    /// both reading "Rain" is worse than useless: one runs and one cannot.
    /// </summary>
    public string? Qualifier { get; internal set; }

    public string DisplayTitle => Qualifier is null ? Title : $"{Title} ({Qualifier})";
    public required string FormatName { get; init; }

    /// <summary>False for 16-bit modules: no 64-bit Windows can load them at all.</summary>
    public required bool CanRun { get; init; }

    public string? CannotRunReason { get; init; }
    public IReadOnlyList<ModuleControl> Controls { get; init; } = [];
    public IReadOnlyList<ModuleControl> Configurable =>
        Controls.Where(c => c.IsConfigurable).ToList();

    public int SoundCount { get; init; }
    public bool HasPalette { get; init; }

    public override string ToString() => $"{DisplayTitle} ({FileName})";
}

/// <summary>Scans a folder and describes every module in it. Executes nothing.</summary>
public static class ModuleCatalog
{
    public static AdModule? Describe(string path)
    {
        ImageReader img;
        try { img = ImageReader.Open(path); }
        catch (InvalidDataException) { return null; }

        if (!img.HasModuleEntryPoint) return null;

        var gen = img.Format == ImageFormat.Ne ? Generation.Classic : Generation.Ad4;
        return new AdModule
        {
            Path = path,
            FileName = System.IO.Path.GetFileName(path),
            Title = DisplayName(img, path),
            Credits = Credits(img),
            Generation = gen,
            FormatName = img.FormatName,
            CanRun = img.IsLoadable,
            CannotRunReason = img.IsLoadable ? null
                : "16-bit module — no 64-bit Windows can load this in any process.",
            Controls = img.Controls(),
            SoundCount = 0,
            HasPalette = img.Resource("PAL") is not null,
        };
    }

    public static List<AdModule> Scan(string folder)
    {
        var found = new List<AdModule>();
        if (!Directory.Exists(folder)) return found;
        foreach (var f in Directory.EnumerateFiles(folder, "*.AD", SearchOption.AllDirectories)
                                   .OrderBy(f => f, StringComparer.OrdinalIgnoreCase))
        {
            try { if (Describe(f) is { } m) found.Add(m); }
            catch { /* one bad file must not stop the scan */ }
        }
        Disambiguate(found);
        return found;
    }

    /// <summary>
    /// Qualify titles that appear more than once, so "Rain" becomes
    /// "Rain (AD40)" and "Rain (Classic)". Only ambiguous names are touched;
    /// a unique title is left exactly as the module states it.
    /// </summary>
    public static void Disambiguate(IEnumerable<AdModule> modules)
    {
        foreach (var group in modules.GroupBy(m => m.Title, StringComparer.OrdinalIgnoreCase))
        {
            var items = group.ToList();
            if (items.Count < 2)
            {
                foreach (var m in items) m.Qualifier = null;
                continue;
            }

            foreach (var byGen in items.GroupBy(m => m.GenerationLabel))
            {
                var same = byGen.ToList();
                // Generation alone usually separates them; if not, fall back to
                // the file name, which is unique within a folder by definition.
                foreach (var m in same)
                    m.Qualifier = same.Count == 1
                        ? byGen.Key
                        : $"{byGen.Key}, {Path.GetFileNameWithoutExtension(m.FileName)}";
            }
        }
    }

    /// <summary>
    /// AD4 modules name themselves in VERSION/FileDescription, with STRINGLIST
    /// 128 as a second source. Classic modules use the MNAME resource (type
    /// 2000, name 20); failing that the NE resident name beats a filename.
    /// </summary>
    private static string DisplayName(ImageReader img, string path)
    {
        if (img.Format != ImageFormat.Ne)
        {
            var v = VersionStrings(img);
            foreach (var key in new[] { "FileDescription", "ProductName" })
                if (v.TryGetValue(key, out var s) && s.Length > 0 &&
                    !s.Contains("Graphics Module Library", StringComparison.OrdinalIgnoreCase))
                    return s;
            if (StringList(img.Resource("STRINGLIST", "#128")) is { Length: > 0 } sl) return sl;
        }
        else
        {
            if (img.Resource("#2000", "#20") is { } mname)
            {
                var s = Latin1(mname);
                if (s.Length > 0) return s;
            }
            var n = img.NeModuleName();
            if (n.Length > 0) return Capitalise(n);
        }
        return System.IO.Path.GetFileNameWithoutExtension(path);
    }

    private static string Credits(ImageReader img)
    {
        if (img.Format == ImageFormat.Ne)
        {
            if (img.Resource("#2000", "#10") is { } cstr)
                return string.Join(' ', Latin1(cstr).Split((char[]?)null,
                    StringSplitOptions.RemoveEmptyEntries));
            return img.NeDescription();
        }
        var v = VersionStrings(img);
        foreach (var key in new[] { "LegalCopyright", "CompanyName" })
            if (v.TryGetValue(key, out var s) && s.Length > 0) return s;
        return "";
    }

    private static string? StringList(byte[]? blob)
    {
        if (blob is null || blob.Length < 3) return null;
        return Latin1(blob.AsSpan(2).ToArray());
    }

    private static string Latin1(byte[] b)
    {
        int end = Array.IndexOf(b, (byte)0);
        if (end < 0) end = b.Length;
        return Encoding.Latin1.GetString(b, 0, end).Trim();
    }

    private static string Capitalise(string s) =>
        s.Length == 0 ? s : char.ToUpperInvariant(s[0]) + s[1..].ToLowerInvariant();

    private static Dictionary<string, string> VersionStrings(ImageReader img)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var blob = img.Resource("#16", "#1");
        if (blob is null) return result;

        var marker = Encoding.Unicode.GetBytes("StringFileInfo");
        int anchor = IndexOf(blob, marker);
        if (anchor < 0) return result;

        int p = Align(anchor + marker.Length);
        while (p + 6 <= blob.Length)
        {
            ushort length = BitConverter.ToUInt16(blob, p);
            ushort valLen = BitConverter.ToUInt16(blob, p + 2);
            ushort type = BitConverter.ToUInt16(blob, p + 4);
            if (length == 0) break;

            int q = p + 6, keyEnd = q;
            while (keyEnd + 1 < blob.Length && !(blob[keyEnd] == 0 && blob[keyEnd + 1] == 0)) keyEnd += 2;
            var key = Encoding.Unicode.GetString(blob, q, keyEnd - q);
            int r = Align(keyEnd + 2);

            if (type == 1 && valLen > 0)
            {
                int bytes = Math.Min(valLen * 2, blob.Length - r);
                if (bytes > 0)
                {
                    var val = Encoding.Unicode.GetString(blob, r, bytes).TrimEnd('\0');
                    if (key.Length > 0 && val.Length > 0) result[key] = val;
                }
                p += Align(length);
            }
            else p = r;   // descend into StringTable
        }
        return result;

        static int Align(int x) => (x + 3) & ~3;
    }

    private static int IndexOf(byte[] haystack, byte[] needle)
    {
        for (int i = 0; i + needle.Length <= haystack.Length; i++)
        {
            bool hit = true;
            for (int j = 0; j < needle.Length; j++)
                if (haystack[i + j] != needle[j]) { hit = false; break; }
            if (hit) return i;
        }
        return -1;
    }
}
