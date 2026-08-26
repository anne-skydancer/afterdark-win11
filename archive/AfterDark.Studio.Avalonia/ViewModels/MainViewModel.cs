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

    /// <summary>Raised on a real change, so the preview restarts with the new value.</summary>
    public event Action? Changed;

    public int Value
    {
        get => _value;
        set
        {
            if (!Set(ref _value, value)) return;
            Raise(nameof(SelectedOption));
            Raise(nameof(IsChecked));
            Raise(nameof(Summary));
            Changed?.Invoke();
        }
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
            var cvm = new ControlViewModel(c, v);
            cvm.Changed += () => ControlChanged?.Invoke();
            Controls.Add(cvm);
        }
    }

    /// <summary>Any of this module's controls changed value.</summary>
    public event Action? ControlChanged;

    public string Title => Module.DisplayTitle;
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
    public event Action? CatalogChanged;
    public event Action? ScreenSaverChanged;

    public MainViewModel(StudioSettings settings, IEnumerable<AdModule> modules)
    {
        _settings = settings;
        _installPath = settings.InstallPath ?? "";
        LoadModules(modules, settings.SelectedModule);
    }

    public void LoadModules(IEnumerable<AdModule> modules, string? selectedFileName = null)
    {
        Selected = null;
        All.Clear();
        foreach (var m in modules)
        {
            var saved = _settings.Modules.TryGetValue(m.FileName, out var ms)
                ? ms.Presets.FirstOrDefault(p => p.Name == ms.ActivePreset)?.Values
                : null;
            All.Add(new ModuleViewModel(m, saved));
        }
        ApplyFilter();
        Selected = Visible.FirstOrDefault(v =>
                       v.FileName.Equals(selectedFileName, StringComparison.OrdinalIgnoreCase))
                   ?? Visible.FirstOrDefault(v => v.CanRun)
                   ?? Visible.FirstOrDefault();
        Raise(nameof(HasModules));
        Raise(nameof(HasNoModules));
        CatalogChanged?.Invoke();
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
        set
        {
            if (_selected is not null) _selected.ControlChanged -= NotifySettingsChanged;
            if (!Set(ref _selected, value)) return;
            if (_selected is not null) _selected.ControlChanged += NotifySettingsChanged;
            Raise(nameof(HasSelection));
            Raise(nameof(CanSetScreenSaver));
            Raise(nameof(SelectionIs16Bit));
        }
    }

    public bool HasSelection => _selected is not null;
    public bool CanSetScreenSaver => _selected is { CanRun: true };
    public bool HasModules => All.Count > 0;
    public bool HasNoModules => All.Count == 0;
    public bool SelectionIs16Bit => _selected is { CanRun: false };

    public int RunnableCount => All.Count(m => m.CanRun);
    public int LegacyCount => All.Count - RunnableCount;

    public string CountLine =>
        $"{Visible.Count} shown · {RunnableCount} runnable · {LegacyCount} legacy 16-bit";

    /// <summary>Preview pacing. Lower than the screensaver's, to stay cheap.</summary>
    public int PreviewFps => Math.Clamp(_settings.TargetFps, 5, 60);

    /// <summary>Raised when any control value changes, so the preview can restart.</summary>
    public event Action? SettingsChanged;

    internal void NotifySettingsChanged() => SettingsChanged?.Invoke();

    private string _status = "";
    public string Status { get => _status; private set => Set(ref _status, value); }

    public void SetImportedModules(string installPath, IEnumerable<AdModule> modules,
                                   int count, int pictureCount = 0, int musicCount = 0)
    {
        InstallPath = installPath;
        _settings.InstallPath = installPath;
        LoadModules(modules, _settings.SelectedModule);
        _settings.Save();
        Status = $"Imported {count} module{(count == 1 ? "" : "s")}"
               + (pictureCount > 0
                   ? $" and {pictureCount} Art Critic picture{(pictureCount == 1 ? "" : "s")}" : "")
               + (musicCount > 0
                   ? $" and {musicCount} music track{(musicCount == 1 ? "" : "s")}" : "")
               + " from your media.";
    }

    public void ReportError(string message) => Status = message;

    /// <summary>
    /// The whole install flow: save settings, project them into saver.cfg, and
    /// point Windows at our .scr. No elevation, no System32, no .reg import.
    /// </summary>
    public void SetAsScreenSaver()
    {
        if (_selected is not { CanRun: true }) return;
        try
        {
            SaveSelection();

            Services.SaverConfig.Write(
                InstallPath,
                _selected.Module.Path,
                _selected.ToValues(),
                _settings,
                Services.AppPaths.Studio);

            // Optionally publish the same choice as the machine-wide default, so
            // every other account gets a working screensaver without setting one
            // up. Silently a no-op without administrator rights.
            if (ApplyToAllUsers) TryWriteMachineDefault();

            if (OperatingSystem.IsWindows())
            {
                Services.ScreenSaverRegistration.Install(Services.AppPaths.ScreenSaver);
                Status = $"{_selected.Title} is now your screensaver."
                       + (ApplyToAllUsers && _machineDefaultWritten
                          ? " Set as the default for other users too." : "");
            }
            else Status = "Settings saved (registration is Windows-only).";
            ScreenSaverChanged?.Invoke();
        }
        catch (Exception ex)
        {
            Status = $"Could not set the screensaver: {ex.Message}";
        }
    }

    private bool _applyToAllUsers;
    private bool _machineDefaultWritten;

    /// <summary>
    /// Publish this choice as the machine-wide default. Which screensaver is
    /// *active* stays per-user -- Windows has no machine-wide equivalent -- but
    /// this gives every account the same module and settings to start from.
    /// </summary>
    public bool ApplyToAllUsers
    {
        get => _applyToAllUsers;
        set => Set(ref _applyToAllUsers, value);
    }

    public bool CanApplyToAllUsers => Services.AppPaths.CanWriteMachineDefaults();

    private void TryWriteMachineDefault()
    {
        _machineDefaultWritten = false;
        if (_selected is null) return;
        try
        {
            Services.SaverConfig.Write(
                InstallPath,
                _selected.Module.Path,
                _selected.ToValues(),
                _settings,
                Services.AppPaths.Studio,
                path: Services.SaverConfig.MachinePath);
            _machineDefaultWritten = true;
        }
        catch (Exception ex) when (ex is UnauthorizedAccessException or IOException)
        {
            Status = "Your screensaver is set. Setting the default for all users "
                   + "needs administrator rights.";
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
