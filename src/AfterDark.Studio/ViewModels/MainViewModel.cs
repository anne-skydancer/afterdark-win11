using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using AfterDark.Catalog;
using AfterDark.Studio.Models;

namespace AfterDark.Studio.ViewModels;

public class Bindable : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;
    protected void Raise([CallerMemberName] string? n = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
    protected bool Set<T>(ref T field, T value, [CallerMemberName] string? n = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value; Raise(n); return true;
    }
}

/// <summary>One control, bound to a live value.</summary>
public sealed class ControlViewModel : Bindable
{
    private readonly ModuleControl _c;
    private int _value;

    public ControlViewModel(ModuleControl c, int value)
    {
        _c = c;
        _value = value;
    }

    public ModuleControl Control => _c;
    public int Slot => _c.Slot;
    public string Title => _c.DisplayTitle;
    public ControlKind Kind => _c.Kind;

    public bool IsChoice   => _c.Kind is ControlKind.StringSlider or ControlKind.ComboBox;
    public bool IsToggle   => _c.Kind is ControlKind.CheckBox;
    public bool IsNumber   => _c.Kind is ControlKind.NumberSlider;

    public IReadOnlyList<ControlOption> Options => _c.Options;

    public int Value
    {
        get => _value;
        set { if (Set(ref _value, value)) { Raise(nameof(SelectedOption)); Raise(nameof(IsChecked)); Raise(nameof(Summary)); } }
    }

    // Every row instantiates all three editors and hides the inapplicable ones.
    // A hidden control still initialises and, with a two-way binding, writes its
    // own default back -- which would silently reset Value to 0. Each setter
    // therefore ignores writes that do not belong to its control kind.

    public ControlOption? SelectedOption
    {
        get => _c.Options.FirstOrDefault(o => o.Value == _value) ?? _c.Options.FirstOrDefault();
        set { if (IsChoice && value is not null) Value = value.Value; }
    }

    public bool IsChecked
    {
        get => _value != 0;
        set { if (IsToggle) Value = value ? 1 : 0; }
    }

    public double Number
    {
        get => _value;
        set { if (IsNumber) Value = (int)Math.Round(value); }
    }

    public int Minimum => _c.Lower;
    public int Maximum => _c.Upper > _c.Lower ? _c.Upper : _c.Lower + 1;

    public string Summary => _c.Kind switch
    {
        ControlKind.CheckBox => IsChecked ? "On" : "Off",
        ControlKind.NumberSlider => $"{_value}{(_c.Affix.Length > 0 ? " " + _c.Affix : "")}",
        _ => SelectedOption?.Label ?? _value.ToString(),
    };
}

public sealed class ModuleViewModel : Bindable
{
    public AdModule Module { get; }
    public ObservableCollection<ControlViewModel> Controls { get; } = [];

    public ModuleViewModel(AdModule m, int[]? values = null)
    {
        Module = m;
        foreach (var c in m.Configurable)
        {
            int v = values is not null && c.Slot < values.Length ? values[c.Slot] : c.DefaultValue;
            Controls.Add(new ControlViewModel(c, v));
        }
    }

    public string Title => Module.Title;
    public string FileName => Module.FileName;
    public string Credits => Module.Credits;
    public bool CanRun => Module.CanRun;
    public string Badge => Module.CanRun ? "AD4" : "16-bit";
    public Avalonia.Media.IBrush BadgeBrush => new Avalonia.Media.SolidColorBrush(
        Avalonia.Media.Color.Parse(Module.CanRun ? "#5BC98B" : "#E4744F"));
    public string StatusLine => Module.CanRun
        ? $"{Module.FormatName} · {Controls.Count} setting{(Controls.Count == 1 ? "" : "s")}"
        : "Cannot run on 64-bit Windows";
    public bool HasControls => Controls.Count > 0;
    public bool HasNoControls => Controls.Count == 0;

    /// <summary>Snapshot of the four slots, ready for AD_MODULE32.iControlValue[].</summary>
    public int[] ToValues()
    {
        var v = new int[4];
        foreach (var c in Controls) if (c.Slot is >= 0 and < 4) v[c.Slot] = c.Value;
        return v;
    }
}

public sealed class MainViewModel : Bindable
{
    private readonly StudioSettings _settings;
    private ModuleViewModel? _selected;
    private string _installPath = "";
    private string _filter = "";

    public ObservableCollection<ModuleViewModel> All { get; } = [];
    public ObservableCollection<ModuleViewModel> Visible { get; } = [];

    public MainViewModel(StudioSettings settings, IEnumerable<AdModule> modules)
    {
        _settings = settings;
        _installPath = settings.InstallPath ?? "";
        foreach (var m in modules)
        {
            var saved = settings.Modules.TryGetValue(m.FileName, out var ms)
                ? ms.Presets.FirstOrDefault(p => p.Name == ms.ActivePreset)?.Values
                : null;
            All.Add(new ModuleViewModel(m, saved));
        }
        ApplyFilter();
        Selected = Visible.FirstOrDefault(v => v.CanRun) ?? Visible.FirstOrDefault();
    }

    public string InstallPath { get => _installPath; set => Set(ref _installPath, value); }

    public string Filter
    {
        get => _filter;
        set { if (Set(ref _filter, value)) ApplyFilter(); }
    }

    private bool _runnableOnly;
    public bool RunnableOnly
    {
        get => _runnableOnly;
        set { if (Set(ref _runnableOnly, value)) ApplyFilter(); }
    }

    private void ApplyFilter()
    {
        Visible.Clear();
        foreach (var m in All)
        {
            if (_runnableOnly && !m.CanRun) continue;
            if (_filter.Length > 0 &&
                !m.Title.Contains(_filter, StringComparison.OrdinalIgnoreCase) &&
                !m.FileName.Contains(_filter, StringComparison.OrdinalIgnoreCase)) continue;
            Visible.Add(m);
        }
        Raise(nameof(CountLine));
    }

    public ModuleViewModel? Selected
    {
        get => _selected;
        set { if (Set(ref _selected, value)) { Raise(nameof(HasSelection)); Raise(nameof(SelectionIs16Bit)); } }
    }

    public bool HasSelection => _selected is not null;
    public bool SelectionIs16Bit => _selected is { CanRun: false };

    public int RunnableCount => All.Count(m => m.CanRun);
    public int LegacyCount => All.Count - RunnableCount;

    public string CountLine =>
        $"{Visible.Count} shown · {RunnableCount} runnable · {LegacyCount} legacy 16-bit";

    public int TimeoutMinutes
    {
        get => _settings.TimeoutMinutes;
        set { _settings.TimeoutMinutes = value; Raise(); }
    }

    public bool SecureResume
    {
        get => _settings.SecureResume;
        set { _settings.SecureResume = value; Raise(); }
    }

    private string _status = "";
    public string Status { get => _status; private set => Set(ref _status, value); }

    /// <summary>
    /// The whole install flow: save settings, project them into saver.cfg, and
    /// point Windows at our .scr. No elevation, no System32, no .reg import.
    /// </summary>
    public void SetAsScreenSaver(string scrPath, string? studioExe)
    {
        if (_selected is null) return;
        try
        {
            SaveSelection();

            AfterDark.Studio.Services.SaverConfig.Write(
                InstallPath,
                _selected.Module.Path,
                _selected.ToValues(),
                _settings,
                studioExe);

            if (OperatingSystem.IsWindows())
            {
                AfterDark.Studio.Services.ScreenSaverRegistration.Install(
                    scrPath, TimeoutMinutes * 60, SecureResume);
                Status = $"{_selected.Title} is now your screensaver.";
            }
            else Status = "Settings saved (registration is Windows-only).";
        }
        catch (Exception ex)
        {
            Status = $"Could not set the screensaver: {ex.Message}";
        }
    }

    /// <summary>Persist the selected module's current control values as its active preset.</summary>
    public void SaveSelection()
    {
        if (_selected is null) return;
        var ms = _settings.For(_selected.FileName);
        var preset = ms.Presets.FirstOrDefault(p => p.Name == ms.ActivePreset);
        if (preset is null) ms.Presets.Add(preset = new Preset { Name = ms.ActivePreset });
        preset.Values = _selected.ToValues();
        _settings.SelectedModule = _selected.FileName;
        _settings.InstallPath = InstallPath;
        _settings.Save();
    }
}
