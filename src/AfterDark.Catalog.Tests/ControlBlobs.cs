using System.Text;

namespace AfterDark.Catalog.Tests;

/// <summary>
/// Builds control resources byte-for-byte to the SDK's documented layout.
///
/// Synthetic on purpose: the repository must never contain module bytes, and a
/// fixture we construct from the spec is a stronger test anyway — it fails if
/// the decoder drifts from the documented format rather than from one sample.
/// </summary>
internal static class ControlBlobs
{
    private static void Title(byte[] b, string title)
    {
        var t = Encoding.Latin1.GetBytes(title);
        Array.Copy(t, 0, b, 2, Math.Min(t.Length, 14));   // szTitle[14]
        // byte 16 is aNull and must stay zero
    }

    public static byte[] StringSlider(string title, (string Label, short Bound)[] stops, short startPos)
    {
        int n = stops.Length;
        var b = new byte[32 + 16 * n + 2 * n];
        BitConverter.GetBytes((ushort)1).CopyTo(b, 0);            // CTL_STRSLIDER
        Title(b, title);
        BitConverter.GetBytes((short)n).CopyTo(b, 22);            // nNumStrings
        BitConverter.GetBytes(startPos).CopyTo(b, 24);            // nStartPos
        for (int i = 0; i < n; i++)
        {
            var s = Encoding.Latin1.GetBytes(stops[i].Label);
            Array.Copy(s, 0, b, 32 + 16 * i, Math.Min(s.Length, 15));
            BitConverter.GetBytes(stops[i].Bound).CopyTo(b, 32 + 16 * n + 2 * i);
        }
        return b;
    }

    public static byte[] ComboBox(string title, string[] items, short startIndex)
    {
        var b = new byte[32 + 16 * items.Length];
        BitConverter.GetBytes((ushort)3).CopyTo(b, 0);            // CTL_COMBOBOX
        Title(b, title);
        BitConverter.GetBytes((short)items.Length).CopyTo(b, 22);
        BitConverter.GetBytes(startIndex).CopyTo(b, 24);
        for (int i = 0; i < items.Length; i++)
        {
            var s = Encoding.Latin1.GetBytes(items[i]);
            Array.Copy(s, 0, b, 32 + 16 * i, Math.Min(s.Length, 15));
        }
        return b;
    }

    public static byte[] CheckBox(string title, bool checkedByDefault)
    {
        var b = new byte[32];
        BitConverter.GetBytes((ushort)5).CopyTo(b, 0);            // CTL_CHECKBOX
        Title(b, title);
        BitConverter.GetBytes((short)(checkedByDefault ? 1 : 0)).CopyTo(b, 24);
        return b;
    }

    public static byte[] NumberSlider(string title, short lower, short upper,
                                      short intervals, short startPos, string affix = "")
    {
        var b = new byte[56];
        BitConverter.GetBytes((ushort)2).CopyTo(b, 0);            // CTL_NUMSLIDER
        Title(b, title);
        BitConverter.GetBytes(startPos).CopyTo(b, 24);
        var a = Encoding.Latin1.GetBytes(affix);
        Array.Copy(a, 0, b, 32, Math.Min(a.Length, 5));
        BitConverter.GetBytes(lower).CopyTo(b, 48);
        BitConverter.GetBytes(upper).CopyTo(b, 50);
        BitConverter.GetBytes(intervals).CopyTo(b, 52);
        return b;
    }

    public static byte[] None() => new byte[32];

    /// <summary>Resources are padded up to file alignment; decoding must survive it.</summary>
    public static byte[] Padded(byte[] blob, int to)
    {
        if (blob.Length >= to) return blob;
        var b = new byte[to];
        blob.CopyTo(b, 0);
        return b;
    }
}
