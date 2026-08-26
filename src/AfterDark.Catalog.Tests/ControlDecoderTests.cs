namespace AfterDark.Catalog.Tests;

public class ControlDecoderTests
{
    [Fact]
    public void StringSlider_returns_the_previous_bound_not_the_matching_one()
    {
        // This is the rule that decides what value reaches the module, and it is
        // easy to get backwards. CONTROLS.TXT: "The value to be returned is the
        // *previous* nBound value... Since 0 is always assumed as a valid
        // boundary value, it can be returned as well."
        var blob = ControlBlobs.StringSlider("Objects:",
            [("Flight", 0), ("Squadron", 25), ("Air Wing", 50), ("Swarm", 75)], startPos: 40);

        var c = ControlDecoder.Decode(blob, slot: 0);

        Assert.Equal(ControlKind.StringSlider, c.Kind);
        Assert.Equal("Objects:", c.Title);
        Assert.Equal(["Flight", "Squadron", "Air Wing", "Swarm"], c.Options.Select(o => o.Label));
        Assert.Equal([0, 0, 25, 50], c.Options.Select(o => o.Value));
    }

    [Fact]
    public void StringSlider_default_is_the_option_the_start_position_selects()
    {
        var blob = ControlBlobs.StringSlider("Objects:",
            [("Flight", 0), ("Squadron", 25), ("Air Wing", 50), ("Swarm", 75)], startPos: 40);

        // Position 40 sits past the 25 boundary but short of 50 -> "Air Wing".
        Assert.Equal(25, ControlDecoder.Decode(blob, 0).DefaultValue);
    }

    [Fact]
    public void ComboBox_options_are_indices()
    {
        var c = ControlDecoder.Decode(
            ControlBlobs.ComboBox("Toasters:", ["Adults", "Babies", "Random"], 2), 1);

        Assert.Equal(ControlKind.ComboBox, c.Kind);
        Assert.Equal([0, 1, 2], c.Options.Select(o => o.Value));
        Assert.Equal(2, c.DefaultValue);
    }

    [Fact]
    public void CheckBox_default_survives_round_trip()
    {
        Assert.Equal(1, ControlDecoder.Decode(ControlBlobs.CheckBox("Use Masks", true), 2).DefaultValue);
        Assert.Equal(0, ControlDecoder.Decode(ControlBlobs.CheckBox("Use Masks", false), 2).DefaultValue);
    }

    [Fact]
    public void NumberSlider_maps_position_onto_the_number_range()
    {
        // The resource stores a 0..100 position, not the number it denotes.
        var c = ControlDecoder.Decode(
            ControlBlobs.NumberSlider("Number of Cats", lower: 0, upper: 3,
                                      intervals: 4, startPos: 44), 0);

        Assert.Equal(ControlKind.NumberSlider, c.Kind);
        Assert.Equal(44, c.RawPosition);
        Assert.InRange(c.DefaultValue, 0, 3);
        Assert.Equal(0, c.PositionToValue(0));
        Assert.Equal(3, c.PositionToValue(100));
    }

    [Fact]
    public void Title_is_limited_to_the_14_byte_field()
    {
        // "Display Karaoke" does not fit; the module stores "Display Karaok".
        var c = ControlDecoder.Decode(ControlBlobs.CheckBox("Display Karaoke", true), 3);
        Assert.Equal("Display Karaok", c.Title);

        c.TitleOverride = "Display Karaoke";
        Assert.Equal("Display Karaoke", c.DisplayTitle);
        Assert.Equal("Display Karaok", c.Title);   // the override is cosmetic only
    }

    [Fact]
    public void Padding_from_file_alignment_is_ignored()
    {
        var blob = ControlBlobs.StringSlider("Speed",
            [("Real slow", 25), ("Slow", 50), ("Fast!", 100)], 0);
        var padded = ControlBlobs.Padded(blob, 128);

        var a = ControlDecoder.Decode(blob, 0);
        var b = ControlDecoder.Decode(padded, 0);

        Assert.Equal(a.Options.Select(o => o.Label), b.Options.Select(o => o.Label));
        Assert.Equal(a.Options.Select(o => o.Value), b.Options.Select(o => o.Value));
    }

    [Fact]
    public void Empty_and_truncated_blobs_decode_to_None_rather_than_throwing()
    {
        Assert.Equal(ControlKind.None, ControlDecoder.Decode(ControlBlobs.None(), 0).Kind);
        Assert.Equal(ControlKind.None, ControlDecoder.Decode(new byte[7], 0).Kind);
        Assert.Equal(ControlKind.None, ControlDecoder.Decode(ReadOnlySpan<byte>.Empty, 0).Kind);
    }

    [Fact]
    public void A_lying_string_count_cannot_read_past_the_blob()
    {
        var b = new byte[32];
        BitConverter.GetBytes((ushort)1).CopyTo(b, 0);
        BitConverter.GetBytes((short)99).CopyTo(b, 22);   // claims 99 strings, has none

        var c = ControlDecoder.Decode(b, 0);
        Assert.Empty(c.Options);
    }
}
