using System.ComponentModel;
using System.Diagnostics;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using AfterDark.Catalog;
using AfterDark.Studio.Services;
using AfterDark.Studio.ViewModels;

namespace AfterDark.Studio.Views;

public partial class MainWindow : Window
{
    private readonly ModulePreview _preview = new();
    private WriteableBitmap? _bitmap;
    private Image? _image;
    private StackPanel? _message;
    private TextBlock? _messageText;

    // Coalesce bursts of setting changes into one restart. Every change restarts
    // the module, and a slider dragged across ten stops must not start ten hosts.
    private DispatcherTimer? _restart;
    private MainViewModel? _vm;
    public bool HideInsteadOfClose { get; set; }

    public MainWindow()
    {
        AvaloniaXamlLoader.Load(this);

        if (this.FindControl<Button>("SetSaver") is { } b) b.Click += OnSetScreenSaver;
        if (this.FindControl<Button>("OpenScreenSaverSettings") is { } settings)
            settings.Click += OnOpenScreenSaverSettings;
        if (this.FindControl<Button>("ImportModules") is { } import) import.Click += OnImportModules;
        _image       = this.FindControl<Image>("PreviewImage");
        _message     = this.FindControl<StackPanel>("PreviewMessage");
        _messageText = this.FindControl<TextBlock>("PreviewMessageText");

        _preview.FrameReady += OnFrame;
        _preview.Stopped    += OnPreviewStopped;

        _restart = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(400) };
        _restart.Tick += (_, _) => { _restart!.Stop(); StartPreview(); };

        DataContextChanged += (_, _) => Attach(DataContext as MainViewModel);
        Attach(DataContext as MainViewModel);
    }

    private void Attach(MainViewModel? vm)
    {
        if (ReferenceEquals(vm, _vm)) return;
        if (_vm is not null)
        {
            _vm.PropertyChanged -= OnViewModelChanged;
            _vm.SettingsChanged -= QueueRestart;
        }
        _vm = vm;
        if (_vm is not null)
        {
            _vm.PropertyChanged += OnViewModelChanged;
            _vm.SettingsChanged += QueueRestart;
        }
        QueueRestart();
    }

    private void OnViewModelChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName is nameof(MainViewModel.Selected)) QueueRestart();
    }

    /// <summary>Called by the settings editors when a control value changes.</summary>
    public void QueueRestart()
    {
        if (_restart is null) return;
        _restart.Stop();
        _restart.Start();
        Show("Starting preview…");
    }

    private void StartPreview()
    {
        var vm = _vm;
        if (vm?.Selected is not { } module) { Show("Select a module."); return; }

        if (!module.CanRun)
        {
            _preview.Stop();
            Show("This module is 16-bit. Windows cannot run it, but its settings "
                 + "are still readable — see docs/REWRITES.md.");
            return;
        }

        _preview.Start(module.Module, vm.InstallPath, module.ToValues(), vm.PreviewFps);
    }

    private void OnFrame(byte[] bgra, int w, int h)
    {
        // Arrives off the UI thread; copy under the dispatcher and repaint.
        Dispatcher.UIThread.Post(() =>
        {
            if (_image is null) return;
            if (_bitmap is null || _bitmap.PixelSize.Width != w || _bitmap.PixelSize.Height != h)
            {
                _bitmap?.Dispose();
                _bitmap = new WriteableBitmap(new PixelSize(w, h), new Vector(96, 96),
                                              PixelFormat.Bgra8888, AlphaFormat.Opaque);
                _image.Source = _bitmap;
            }

            using (var fb = _bitmap.Lock())
            {
                int rowBytes = w * 4;
                for (int y = 0; y < h; y++)
                    System.Runtime.InteropServices.Marshal.Copy(
                        bgra, y * rowBytes, fb.Address + y * fb.RowBytes, rowBytes);
            }

            HideMessage();
            _image.InvalidateVisual();
        }, DispatcherPriority.Render);
    }

    private void OnPreviewStopped(string reason) =>
        Dispatcher.UIThread.Post(() => Show(reason));

    private void Show(string text)
    {
        if (_messageText is not null) _messageText.Text = text;
        if (_message is not null) _message.IsVisible = true;
        if (_image is not null) _image.IsVisible = false;
    }

    private void HideMessage()
    {
        if (_message is not null) _message.IsVisible = false;
        if (_image is not null) _image.IsVisible = true;
    }

    private void OnSetScreenSaver(object? sender, RoutedEventArgs e)
    {
        // Paths come from Setup's HKLM record, not from wherever a binary sits.
        if (DataContext is MainViewModel vm) vm.SetAsScreenSaver();
    }

    private void OnOpenScreenSaverSettings(object? sender, RoutedEventArgs e)
        => OpenWindowsScreenSaverSettings(DataContext as MainViewModel);

    public static void OpenWindowsScreenSaverSettings(MainViewModel? viewModel)
    {
        if (!OperatingSystem.IsWindows()) return;
        try
        {
            Process.Start(new ProcessStartInfo("control.exe", "desk.cpl,,@screensaver")
            {
                UseShellExecute = true,
            });
        }
        catch (Exception ex) when (ex is InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            viewModel?.ReportError($"Could not open Windows screensaver settings: {ex.Message}");
        }
    }

    protected override void OnClosing(WindowClosingEventArgs e)
    {
        if (HideInsteadOfClose)
        {
            e.Cancel = true;
            Hide();
            _preview.Stop();
            _restart?.Stop();
        }
        base.OnClosing(e);
    }

    private async void OnImportModules(object? sender, RoutedEventArgs e)
    {
        if (DataContext is not MainViewModel vm || sender is not Button button) return;

        var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = "Choose your After Dark disc or installation",
            AllowMultiple = false,
        });
        if (folders.Count == 0) return;

        button.IsEnabled = false;
        try
        {
            var result = await Task.Run(() => ModuleImporter.Import(folders[0].Path.LocalPath));
            if (OperatingSystem.IsWindows())
                LegacyModulePreferences.EnsureArtCriticPath(result.Destination);
            var modules = await Task.Run(() => ModuleCatalog.Scan(result.Destination));
            LabelOverrides.Load(Path.Combine(AppContext.BaseDirectory, "data", "labels.json"))
                          .Apply(modules);
            vm.SetImportedModules(result.Destination, modules,
                                  result.ModuleCount, result.PictureCount, result.MusicCount);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            vm.ReportError($"Could not import modules: {ex.Message}");
        }
        finally
        {
            button.IsEnabled = true;
        }
    }

    protected override void OnClosed(EventArgs e)
    {
        _restart?.Stop();
        if (_vm is not null)
        {
            _vm.PropertyChanged -= OnViewModelChanged;
            _vm.SettingsChanged -= QueueRestart;
        }
        _preview.FrameReady -= OnFrame;
        _preview.Stopped    -= OnPreviewStopped;
        _preview.Dispose();
        _bitmap?.Dispose();
        base.OnClosed(e);
    }
}
