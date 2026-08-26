namespace AfterDark.Catalog;

/// <summary>The kinds of setting an After Dark module can expose.</summary>
public enum ControlKind
{
    None = 0,
    StringSlider = 1,
    NumberSlider = 2,
    ComboBox = 3,
    Button = 4,
    CheckBox = 5,
}

/// <summary>One labelled stop on a string slider, with the value it yields.</summary>
public sealed record ControlOption(string Label, int Value);

/// <summary>
/// One of a module's four settings, decoded from its control resource.
/// <see cref="Slot"/> is the index into AD_MODULE32.iControlValue[], which is
/// how the value actually reaches the module.
/// </summary>
public sealed class ModuleControl
{
    public int Slot { get; init; }
    public ControlKind Kind { get; init; }

    /// <summary>
    /// The module's own label. szTitle is a fixed 14-byte field, so long names
    /// arrive truncated ("Display Karaok"); <see cref="DisplayTitle"/> is where
    /// a friendlier override belongs.
    /// </summary>
    public string Title { get; init; } = "";

    public string? TitleOverride { get; set; }
    public string DisplayTitle => TitleOverride ?? Title;

    public IReadOnlyList<ControlOption> Options { get; init; } = [];
    public int DefaultValue { get; init; }

    // Numeric sliders only.
    public int Lower { get; init; }
    public int Upper { get; init; }
    public int Intervals { get; init; }
    public string Affix { get; init; } = "";

    /// <summary>
    /// Raw 0..100 slider position as stored in the resource. For numeric
    /// sliders the resource records a position, not the number it denotes;
    /// <see cref="DefaultValue"/> carries the mapped number.
    /// The SDK is explicit that numeric sliders have "wrinkles" it declines to
    /// document, so treat the mapping as a good approximation, not gospel.
    /// </summary>
    public int RawPosition { get; init; }

    /// <summary>Map a 0..100 slider position onto this control's number range.</summary>
    public int PositionToValue(int position)
    {
        if (Kind != ControlKind.NumberSlider || Upper == Lower) return position;
        int span = Upper - Lower;
        int steps = Intervals > 1 && Intervals <= 100 ? Intervals - 1 : 100;
        int idx = (int)Math.Round(Math.Clamp(position, 0, 100) / 100.0 * steps);
        return Lower + (int)Math.Round(idx / (double)steps * span);
    }

    public bool IsConfigurable => Kind is ControlKind.StringSlider or ControlKind.NumberSlider
                                       or ControlKind.ComboBox or ControlKind.CheckBox;

    public override string ToString() => $"{Kind} \"{DisplayTitle}\" (slot {Slot})";
}
