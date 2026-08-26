using System.Buffers.Binary;
using System.Text;

namespace AfterDark.Catalog;

/// <summary>
/// Decodes an After Dark control-definition resource.
///
/// Layout is byte-packed with no padding: a 32-byte header, optionally followed
/// by a 16-bytes-per-entry string array, and for string sliders a further
/// 2-bytes-per-entry bounds array. Documented in CONTROLS.TXT in the Berkeley
/// Systems After Dark 3.0 SDK; both module generations use it unchanged.
/// </summary>
public static class ControlDecoder
{
    public const int HeaderSize = 32;
    private const int EntrySize = 16;

    /// <summary>Resources may be padded up to file alignment; trailing bytes are ignored.</summary>
    public static ModuleControl Decode(ReadOnlySpan<byte> blob, int slot)
    {
        if (blob.Length < HeaderSize)
            return new ModuleControl { Slot = slot, Kind = ControlKind.None };

        var kind = (ControlKind)BinaryPrimitives.ReadUInt16LittleEndian(blob);
        var title = Latin1(blob.Slice(2, 14));

        switch (kind)
        {
            case ControlKind.StringSlider:
            {
                int count = Clamp(BinaryPrimitives.ReadInt16LittleEndian(blob[22..]));
                int start = BinaryPrimitives.ReadInt16LittleEndian(blob[24..]);
                var labels = Strings(blob, count);
                int boundsAt = HeaderSize + EntrySize * labels.Count;

                var options = new List<ControlOption>(labels.Count);
                for (int i = 0; i < labels.Count; i++)
                {
                    // The engine returns the *previous* boundary, so option 0 is always 0.
                    int value = 0;
                    if (i > 0)
                    {
                        int at = boundsAt + 2 * (i - 1);
                        if (at + 2 > blob.Length) break;
                        value = BinaryPrimitives.ReadInt16LittleEndian(blob[at..]);
                    }
                    options.Add(new ControlOption(labels[i], value));
                }

                return new ModuleControl
                {
                    Slot = slot, Kind = kind, Title = title,
                    Options = options,
                    RawPosition = start,
                    DefaultValue = NearestOptionValue(options, start),
                };
            }

            case ControlKind.ComboBox:
            {
                int count = Clamp(BinaryPrimitives.ReadInt16LittleEndian(blob[22..]));
                int start = BinaryPrimitives.ReadInt16LittleEndian(blob[24..]);
                var labels = Strings(blob, count);
                var options = labels.Select((l, i) => new ControlOption(l, i)).ToList();
                return new ModuleControl
                {
                    Slot = slot, Kind = kind, Title = title,
                    Options = options,
                    DefaultValue = start >= 0 && start < options.Count ? start : 0,
                };
            }

            case ControlKind.NumberSlider:
            {
                int start = BinaryPrimitives.ReadInt16LittleEndian(blob[24..]);
                var affix = Latin1(blob.Slice(32, Math.Min(6, blob.Length - 32)));
                int lower = 0, upper = 0, intervals = 0;
                if (blob.Length >= 56)
                {
                    lower = BinaryPrimitives.ReadInt16LittleEndian(blob[48..]);
                    upper = BinaryPrimitives.ReadInt16LittleEndian(blob[50..]);
                    intervals = BinaryPrimitives.ReadInt16LittleEndian(blob[52..]);
                }
                var numeric = new ModuleControl
                {
                    Slot = slot, Kind = kind, Title = title, Affix = affix,
                    Lower = lower, Upper = upper, Intervals = intervals,
                    RawPosition = start,
                };
                return new ModuleControl
                {
                    Slot = slot, Kind = kind, Title = title, Affix = affix,
                    Lower = lower, Upper = upper, Intervals = intervals,
                    RawPosition = start,
                    DefaultValue = numeric.PositionToValue(start),
                };
            }

            case ControlKind.CheckBox:
                return new ModuleControl
                {
                    Slot = slot, Kind = kind, Title = title,
                    DefaultValue = BinaryPrimitives.ReadInt16LittleEndian(blob[24..]) != 0 ? 1 : 0,
                };

            case ControlKind.Button:
                return new ModuleControl { Slot = slot, Kind = kind, Title = title };

            default:
                return new ModuleControl { Slot = slot, Kind = ControlKind.None, Title = title };
        }
    }

    private static int Clamp(int n) => Math.Clamp(n, 0, 100);

    private static List<string> Strings(ReadOnlySpan<byte> blob, int count)
    {
        var result = new List<string>(count);
        for (int i = 0; i < count; i++)
        {
            int at = HeaderSize + EntrySize * i;
            if (at + EntrySize > blob.Length) break;
            result.Add(Latin1(blob.Slice(at, EntrySize)));
        }
        return result;
    }

    /// <summary>Map a raw 0..100 slider position onto the option it selects.</summary>
    private static int NearestOptionValue(List<ControlOption> options, int position)
    {
        int chosen = 0;
        foreach (var o in options)
            if (position >= o.Value) chosen = o.Value;
        return chosen;
    }

    private static string Latin1(ReadOnlySpan<byte> span)
    {
        int end = span.IndexOf((byte)0);
        if (end >= 0) span = span[..end];
        return Encoding.Latin1.GetString(span).Trim();
    }
}
