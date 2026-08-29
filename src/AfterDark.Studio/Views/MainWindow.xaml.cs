using System.ComponentModel;
using System.Diagnostics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using AfterDark.Catalog;
using AfterDark.Studio.Services;
using AfterDark.Studio.ViewModels;
using Microsoft.Win32;

namespace AfterDark.Studio.Views;

public partial class MainWindow : Window
{
    private readonly ModulePreview _preview = new();
    private WriteableBitmap? _bitmap;
    private readonly DispatcherTimer _restart;
    private MainViewModel? _viewModel;

    public bool HideInsteadOfClose { get; set; }

    public MainWindow()
    {
        InitializeComponent();

        _preview.FrameReady += OnFrame;
        _preview.Stopped += OnPreviewStopped;
        _restart = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(400) };
        _restart.Tick += (_, _) =>
        {
            _restart.Stop();
            StartPreview();
        };

        DataContextChanged += (_, args) => Attach(args.NewValue as MainViewModel);
        Loaded += (_, _) => Attach(DataContext as MainViewModel);
    }

    private void Attach(MainViewModel? viewModel)
    {
        if (ReferenceEquals(viewModel, _viewModel)) return;
        if (_viewModel is not null)
        {
            _viewModel.PropertyChanged -= OnViewModelChanged;
            _viewModel.SettingsChanged -= QueueRestart;
        }

        _viewModel = viewModel;
        if (_viewModel is not null)
        {
            _viewModel.PropertyChanged += OnViewModelChanged;
            _viewModel.SettingsChanged += QueueRestart;
        }
        QueueRestart();
    }

    private void OnViewModelChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName is nameof(MainViewModel.Selected)) QueueRestart();
    }

    public void QueueRestart()
    {
        _restart.Stop();
        _restart.Start();
        ShowMessage("Starting preview…");
    }

    private void StartPreview()
    {
        if (_viewModel?.Selected is not { } module)
        {
            ShowMessage("Select a module.");
            return;
        }
        if (!module.CanRun)
        {
            _preview.Stop();
            ShowMessage("This module is 16-bit. Windows cannot run it, but its settings " +
                        "are still readable — see docs/REWRITES.md.");
            return;
        }

        _preview.Start(module.Module, _viewModel.InstallPath,
                       module.ToValues(), _viewModel.PreviewFps);
    }

    private void OnFrame(byte[] bgra, int width, int height)
    {
        Dispatcher.BeginInvoke(DispatcherPriority.Render, () =>
        {
            if (_bitmap is null || _bitmap.PixelWidth != width || _bitmap.PixelHeight != height)
            {
                _bitmap = new WriteableBitmap(width, height, 96, 96, PixelFormats.Bgra32, null);
                PreviewImage.Source = _bitmap;
            }

            _bitmap.WritePixels(new Int32Rect(0, 0, width, height), bgra, width * 4, 0);
            PreviewMessage.Visibility = Visibility.Collapsed;
            PreviewImage.Visibility = Visibility.Visible;
        });
    }

    private void OnPreviewStopped(string reason) =>
        Dispatcher.BeginInvoke(() => ShowMessage(reason));

    private void ShowMessage(string text)
    {
        PreviewMessageText.Text = text;
        PreviewMessage.Visibility = Visibility.Visible;
        PreviewImage.Visibility = Visibility.Collapsed;
    }

    private void OnSetScreenSaver(object sender, RoutedEventArgs e)
    {
        if (DataContext is MainViewModel viewModel) viewModel.SetAsScreenSaver();
    }

    private void OnOpenScreenSaverSettings(object sender, RoutedEventArgs e) =>
        OpenWindowsScreenSaverSettings(DataContext as MainViewModel);

    public static void OpenWindowsScreenSaverSettings(MainViewModel? viewModel)
    {
        try
        {
            Process.Start(new ProcessStartInfo("control.exe", "desk.cpl,,@screensaver")
            {
                UseShellExecute = true,
            });
        }
        catch (Exception ex) when (ex is InvalidOperationException or Win32Exception)
        {
            viewModel?.ReportError($"Could not open Windows screensaver settings: {ex.Message}");
        }
    }

    private async void OnImportModules(object sender, RoutedEventArgs e)
    {
        if (DataContext is not MainViewModel viewModel ||
            sender is not System.Windows.Controls.Button button) return;

        var dialog = new OpenFolderDialog
        {
            Title = "Choose your After Dark disc or installation",
            Multiselect = false,
        };
        if (dialog.ShowDialog(this) != true) return;

        button.IsEnabled = false;
        try
        {
            var result = await Task.Run(() => ModuleImporter.Import(dialog.FolderName));
            LegacyModulePreferences.EnsureArtCriticPath(result.Destination);
            var modules = await Task.Run(() => ModuleCatalog.Scan(result.Destination));
            LabelOverrides.Load(Path.Combine(AppContext.BaseDirectory, "data", "labels.json"))
                          .Apply(modules);
            viewModel.SetImportedModules(result.Destination, modules,
                result.ModuleCount, result.PictureCount, result.MusicCount);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            viewModel.ReportError($"Could not import modules: {ex.Message}");
        }
        finally { button.IsEnabled = true; }
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        if (HideInsteadOfClose)
        {
            e.Cancel = true;
            Hide();
            _preview.Stop();
            _restart.Stop();
        }
        base.OnClosing(e);
    }

    protected override void OnClosed(EventArgs e)
    {
        _restart.Stop();
        if (_viewModel is not null)
        {
            _viewModel.PropertyChanged -= OnViewModelChanged;
            _viewModel.SettingsChanged -= QueueRestart;
        }
        _preview.FrameReady -= OnFrame;
        _preview.Stopped -= OnPreviewStopped;
        _preview.Dispose();
        base.OnClosed(e);
    }

    public void CaptureToPng(string path)
    {
        UpdateLayout();
        var dpi = VisualTreeHelper.GetDpi(this);
        int width = Math.Max(1, (int)Math.Ceiling(ActualWidth * dpi.DpiScaleX));
        int height = Math.Max(1, (int)Math.Ceiling(ActualHeight * dpi.DpiScaleY));
        var bitmap = new RenderTargetBitmap(width, height,
            dpi.PixelsPerInchX, dpi.PixelsPerInchY, PixelFormats.Pbgra32);
        bitmap.Render(this);

        var encoder = new PngBitmapEncoder();
        encoder.Frames.Add(BitmapFrame.Create(bitmap));
        using var stream = File.Create(path);
        encoder.Save(stream);
    }
}