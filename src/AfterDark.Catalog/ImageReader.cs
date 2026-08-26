using System.Buffers.Binary;
using System.Text;

namespace AfterDark.Catalog;

public enum ImageFormat { Unknown, Pe32, Pe64, Ne }

/// <summary>
/// Read-only reader for the two After Dark module generations. Parses headers
/// and resources; never loads or executes a module.
/// </summary>
public sealed class ImageReader
{
    // Both generations keep the four control definitions in resource type 1000,
    // names 1..4. The SDK ships them as loose CTRL1..CTRL4.RSC, but the .RC
    // script compiles them into type 1000.
    public const string ControlType = "#1000";
    private static readonly string[] ControlNames = ["#1", "#2", "#3", "#4"];

    private readonly byte[] _d;

    public ImageFormat Format { get; }
    public bool IsDll { get; }

    private readonly int _peOffset;
    private readonly int _dataDirs;
    private readonly List<(uint Va, uint VSize, uint Raw, uint RawSize)> _sections = [];
    private readonly int _neOffset;

    private ImageReader(byte[] data)
    {
        _d = data;
        if (data.Length < 0x40 || data[0] != 'M' || data[1] != 'Z')
            throw new InvalidDataException("not an MZ image");

        int nt = BinaryPrimitives.ReadInt32LittleEndian(_d.AsSpan(0x3C));
        if (nt > 0 && nt + 4 < data.Length && _d[nt] == 'P' && _d[nt + 1] == 'E')
        {
            _peOffset = nt;
            ushort machine = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(nt + 4));
            ushort nsec = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(nt + 6));
            ushort optSize = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(nt + 20));
            ushort chars = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(nt + 22));
            int opt = nt + 24;
            bool plus = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(opt)) == 0x20B;
            _dataDirs = opt + (plus ? 112 : 96);
            IsDll = (chars & 0x2000) != 0;
            Format = machine == 0x14C ? ImageFormat.Pe32
                   : plus ? ImageFormat.Pe64 : ImageFormat.Pe32;
            for (int i = 0; i < nsec; i++)
            {
                int b = opt + optSize + i * 40;
                if (b + 40 > _d.Length) break;
                _sections.Add((
                    BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(b + 12)),
                    BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(b + 8)),
                    BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(b + 20)),
                    BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(b + 16))));
            }
            return;
        }
        if (nt > 0 && nt + 2 < data.Length && _d[nt] == 'N' && _d[nt + 1] == 'E')
        {
            _neOffset = nt;
            Format = ImageFormat.Ne;
            IsDll = (BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(nt + 0x0C)) & 0x8000) != 0;
            return;
        }
        throw new InvalidDataException("neither PE nor NE");
    }

    public static ImageReader Open(string path) => new(File.ReadAllBytes(path));
    public static ImageReader FromBytes(byte[] data) => new(data);

    /// <summary>True if a 32-bit host process on 64-bit Windows can load this.</summary>
    public bool IsLoadable => Format == ImageFormat.Pe32;

    public string FormatName => Format switch
    {
        ImageFormat.Pe32 => "PE32 (i386)",
        ImageFormat.Pe64 => "PE32+ (x64)",
        ImageFormat.Ne   => "NE (16-bit Windows)",
        _ => "unknown",
    };

    // ---------------------------------------------------------------- exports

    public IReadOnlyList<string> Exports()
    {
        if (Format == ImageFormat.Ne) return NeExports();
        var (rva, _) = Dir(0);
        if (rva == 0) return [];
        int b = Off(rva);
        if (b < 0) return [];
        int count = BinaryPrimitives.ReadInt32LittleEndian(_d.AsSpan(b + 24));
        int table = Off(BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(b + 32)));
        if (table < 0) return [];
        var names = new List<string>(count);
        for (int i = 0; i < count; i++)
        {
            int o = Off(BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(table + 4 * i)));
            if (o >= 0) names.Add(CStr(o));
        }
        return names;
    }

    /// <summary>
    /// A module exports its entry point either undecorated (the Borland-built
    /// modules that shipped) or with the MSVC __stdcall decoration.
    /// AFTERDAR.SCR tries the decorated name first and falls back; STARRYNI.AD,
    /// the self-contained default in ENGINE/, is the one that needs it.
    /// </summary>
    public static bool IsModuleEntryPoint(string name) =>
        name.Equals("Module", StringComparison.OrdinalIgnoreCase) ||
        name.Equals("_Module@4", StringComparison.OrdinalIgnoreCase);

    public bool HasModuleEntryPoint => Exports().Any(IsModuleEntryPoint);

    // -------------------------------------------------------------- resources

    /// <summary>Raw bytes of one resource, or null.</summary>
    public byte[]? Resource(string type, string? name = null)
        => Format == ImageFormat.Ne ? NeResource(type, name) : PeResource(type, name);

    public IReadOnlyList<ModuleControl> Controls()
    {
        var result = new List<ModuleControl>(4);
        for (int i = 0; i < 4; i++)
        {
            var blob = Resource(ControlType, ControlNames[i]);
            result.Add(blob is null
                ? new ModuleControl { Slot = i, Kind = ControlKind.None }
                : ControlDecoder.Decode(blob, i));
        }
        return result;
    }

    // ---------------------------------------------------------------- PE bits

    private (uint Rva, uint Size) Dir(int i)
    {
        int at = _dataDirs + i * 8;
        if (at + 8 > _d.Length) return (0, 0);
        return (BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(at)),
                BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(at + 4)));
    }

    private int Off(uint rva)
    {
        foreach (var s in _sections)
            if (rva >= s.Va && rva < s.Va + Math.Max(s.VSize, s.RawSize))
            {
                uint delta = rva - s.Va;
                if (delta < s.RawSize) return (int)(s.Raw + delta);
            }
        return -1;
    }

    private string CStr(int off)
    {
        int end = off;
        while (end < _d.Length && _d[end] != 0) end++;
        return Encoding.Latin1.GetString(_d, off, end - off);
    }

    private IEnumerable<(uint Id, uint Offset)> DirEntries(int at)
    {
        if (at + 16 > _d.Length) yield break;
        int named = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(at + 12));
        int ids = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(at + 14));
        for (int i = 0; i < named + ids; i++)
        {
            int b = at + 16 + 8 * i;
            if (b + 8 > _d.Length) yield break;
            yield return (BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(b)),
                          BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(b + 4)));
        }
    }

    private string PeName(int root, uint value)
    {
        if ((value & 0x80000000) == 0) return "#" + value;
        int o = root + (int)(value & 0x7FFFFFFF);
        if (o + 2 > _d.Length) return "#?";
        int len = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(o));
        return Encoding.Unicode.GetString(_d, o + 2, Math.Min(len * 2, _d.Length - o - 2));
    }

    private byte[]? PeResource(string type, string? name)
    {
        var (rva, _) = Dir(2);
        if (rva == 0) return null;
        int root = Off(rva);
        if (root < 0) return null;

        foreach (var (tv, to) in DirEntries(root))
        {
            if (!PeName(root, tv).Equals(type, StringComparison.OrdinalIgnoreCase)) continue;
            if ((to & 0x80000000) == 0) continue;
            foreach (var (nv, no) in DirEntries(root + (int)(to & 0x7FFFFFFF)))
            {
                if (name is not null && !PeName(root, nv).Equals(name, StringComparison.OrdinalIgnoreCase)) continue;
                if ((no & 0x80000000) == 0) continue;
                foreach (var (_, lo) in DirEntries(root + (int)(no & 0x7FFFFFFF)))
                {
                    if ((lo & 0x80000000) != 0) continue;
                    int e = root + (int)lo;
                    if (e + 8 > _d.Length) continue;
                    uint dataRva = BinaryPrimitives.ReadUInt32LittleEndian(_d.AsSpan(e));
                    int size = BinaryPrimitives.ReadInt32LittleEndian(_d.AsSpan(e + 4));
                    int o = Off(dataRva);
                    if (o < 0 || size <= 0 || o + size > _d.Length) continue;
                    return _d.AsSpan(o, size).ToArray();
                }
            }
        }
        return null;
    }

    // ---------------------------------------------------------------- NE bits

    private List<(string Name, ushort Ordinal)> NeNames(int off)
    {
        var result = new List<(string, ushort)>();
        int p = off;
        while (p > 0 && p < _d.Length)
        {
            int len = _d[p];
            if (len == 0 || p + 1 + len + 2 > _d.Length) break;
            result.Add((Encoding.Latin1.GetString(_d, p + 1, len),
                        BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(p + 1 + len))));
            p += 1 + len + 2;
        }
        return result;
    }

    public string NeModuleName()
    {
        int res = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(_neOffset + 0x26));
        var names = NeNames(_neOffset + res);
        return names.Count > 0 ? names[0].Name : "";
    }

    public string NeDescription()
    {
        int nr = BinaryPrimitives.ReadInt32LittleEndian(_d.AsSpan(_neOffset + 0x2C));
        if (nr <= 0 || nr >= _d.Length) return "";
        var names = NeNames(nr);
        return names.Count > 0 ? names[0].Name : "";
    }

    private IReadOnlyList<string> NeExports()
    {
        var result = new List<string>();
        int res = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(_neOffset + 0x26));
        result.AddRange(NeNames(_neOffset + res).Skip(1).Select(n => n.Name));
        int nr = BinaryPrimitives.ReadInt32LittleEndian(_d.AsSpan(_neOffset + 0x2C));
        if (nr > 0 && nr < _d.Length) result.AddRange(NeNames(nr).Skip(1).Select(n => n.Name));
        return result;
    }

    private static readonly Dictionary<int, string> StdTypes = new()
    {
        [1] = "CURSOR", [2] = "BITMAP", [3] = "ICON", [4] = "MENU", [5] = "DIALOG",
        [6] = "STRING", [8] = "FONT", [10] = "RCDATA", [12] = "GROUP_CURSOR",
        [14] = "GROUP_ICON", [16] = "VERSION",
    };

    private byte[]? NeResource(string type, string? name)
    {
        int rsrc = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(_neOffset + 0x24));
        int restab = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(_neOffset + 0x26));
        if (rsrc == 0 || rsrc == restab) return null;

        int b = _neOffset + rsrc;
        if (b + 2 > _d.Length) return null;
        int shift = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(b));
        if (shift > 16) return null;

        // Integer ids: only resource *types* map to the standard names. A
        // resource *name* of 1 is "#1", not "CURSOR".
        string Label(ushort v, bool isType)
        {
            if ((v & 0x8000) != 0)
            {
                int id = v & 0x7FFF;
                return isType && StdTypes.TryGetValue(id, out var s) ? s : "#" + id;
            }
            int o = b + v;
            if (o >= _d.Length) return "?";
            int len = _d[o];
            return Encoding.Latin1.GetString(_d, o + 1, Math.Min(len, _d.Length - o - 1));
        }

        int p = b + 2;
        while (p + 8 <= _d.Length)
        {
            ushort typeId = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(p));
            if (typeId == 0) break;
            int count = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(p + 2));
            p += 8;
            bool wanted = Label(typeId, true).Equals(type, StringComparison.OrdinalIgnoreCase);
            for (int i = 0; i < count; i++)
            {
                if (p + 12 > _d.Length) return null;
                int off = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(p)) << shift;
                int len = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(p + 2)) << shift;
                ushort rid = BinaryPrimitives.ReadUInt16LittleEndian(_d.AsSpan(p + 6));
                p += 12;
                if (!wanted) continue;
                if (name is not null && !Label(rid, false).Equals(name, StringComparison.OrdinalIgnoreCase)) continue;
                if (off <= 0 || len <= 0 || off + len > _d.Length) continue;
                return _d.AsSpan(off, len).ToArray();
            }
        }
        return null;
    }
}
