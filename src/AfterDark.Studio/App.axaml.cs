using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using AfterDark.Catalog;
using AfterDark.Studio.Models;
using AfterDark.Studio.Services;
using AfterDark.Studio.ViewModels;
using AfterDark.Studio.Views;

namespace AfterDark.Studio;

public partial class App : Application
{
    private TrayIcon? _trayIcon;
    private MainWindow? _mainWindow;
    private MainViewModel? _viewModel;

    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            var settings = StudioSettings.Load();
            var installPath = FindInstallPath(settings);
            settings.InstallPath = installPath;
            if (installPath is not null && OperatingSystem.IsWindows())
            {
                try { LegacyModulePreferences.EnsureArtCriticPath(installPath); }
                catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
                {
                    // Compatibility seeding is optional; never block Studio startup.
                }
            }

            var modules = installPath is null ? [] : ModuleCatalog.Scan(installPath);
            LabelOverrides.Load(Path.Combine(AppContext.BaseDirectory, "data", "labels.json"))
                          .Apply(modules);

            _viewModel = new MainViewModel(settings, modules);
            _mainWindow = new MainWindow
            {
                DataContext = _viewModel,
            };
            _mainWindow.HideInsteadOfClose = true;
            desktop.MainWindow = _mainWindow;
            desktop.ShutdownMode = ShutdownMode.OnExplicitShutdown;
            CreateTrayIcon(desktop);
            _viewModel.CatalogChanged += RefreshTrayMenu;
            _viewModel.ScreenSaverChanged += RefreshTrayMenu;
        }
        base.OnFrameworkInitializationCompleted();
    }

    private void CreateTrayIcon(IClassicDesktopStyleApplicationLifetime desktop)
    {
        if (_mainWindow is null || _viewModel is null) return;

        _trayIcon = new TrayIcon
        {
            Icon = CreateTrayIconImage(),
            ToolTipText = "After Dark Studio",
            Menu = BuildTrayMenu(desktop),
        };
        _trayIcon.Clicked += (_, _) => ShowMainWindow();
        TrayIcon.SetIcons(this, new TrayIcons { _trayIcon });
    }

    private static WindowIcon CreateTrayIconImage()
    {
        const int size = 32;
        var pixels = new byte[size * size * 4];

        void SetPixel(int x, int y, byte red, byte green, byte blue, byte alpha = 0xFF)
        {
            int offset = (y * size + x) * 4;
            pixels[offset + 0] = blue;
            pixels[offset + 1] = green;
            pixels[offset + 2] = red;
            pixels[offset + 3] = alpha;
        }

        // A near-black rounded tile keeps the monogram legible on both light
        // and dark taskbars. The one-pixel amber rim gives it a distinct edge.
        for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
        {
            int edgeX = Math.Min(x, size - 1 - x);
            int edgeY = Math.Min(y, size - 1 - y);
            bool inside = edgeX >= 3 || edgeY >= 3 ||
                          Math.Pow(3 - edgeX, 2) + Math.Pow(3 - edgeY, 2) <= 9;
            if (!inside) continue;
            bool rim = edgeX == 0 || edgeY == 0 ||
                       (edgeX < 3 && edgeY < 3 &&
                        Math.Pow(3 - edgeX, 2) + Math.Pow(3 - edgeY, 2) >= 4);
            SetPixel(x, y, rim ? (byte)0xF2 : (byte)0x1B,
                           rim ? (byte)0xB3 : (byte)0x1D,
                           rim ? (byte)0x3D : (byte)0x27);
        }

        string[] letterA = ["01110", "10001", "10001", "11111", "10001", "10001", "10001"];
        string[] letterD = ["11110", "10001", "10001", "10001", "10001", "10001", "11110"];

        void DrawLetter(string[] rows, int left, byte red, byte green, byte blue)
        {
            for (int row = 0; row < rows.Length; row++)
            for (int column = 0; column < rows[row].Length; column++)
            {
                if (rows[row][column] != '1') continue;
                for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++)
                    SetPixel(left + column * 2 + dx, 9 + row * 2 + dy, red, green, blue);
            }
        }

        DrawLetter(letterA, 5, 0xF2, 0xB3, 0x3D);
        DrawLetter(letterD, 17, 0xF4, 0xF4, 0xF6);

        using var bitmap = new WriteableBitmap(
            new PixelSize(size, size), new Vector(96, 96),
            PixelFormat.Bgra8888, AlphaFormat.Unpremul);
        using (var frameBuffer = bitmap.Lock())
            System.Runtime.InteropServices.Marshal.Copy(pixels, 0, frameBuffer.Address, pixels.Length);
        return new WindowIcon(bitmap);
    }

    private NativeMenu BuildTrayMenu(IClassicDesktopStyleApplicationLifetime desktop)
    {
        var menu = new NativeMenu();

        var open = new NativeMenuItem("Open After Dark Studio");
        open.Click += (_, _) => ShowMainWindow();
        menu.Add(open);
        menu.Add(new NativeMenuItemSeparator());

        var runnableModules = _viewModel!.All.Where(module => module.CanRun).ToList();
        if (runnableModules.Count == 0)
            menu.Add(new NativeMenuItem("No runnable modules") { IsEnabled = false });

        foreach (var module in runnableModules)
        {
            var item = new NativeMenuItem(module.Title)
            {
                ToggleType = NativeMenuItemToggleType.Radio,
                IsChecked = ReferenceEquals(module, _viewModel.Selected),
            };
            item.Click += (_, _) => ActivateFromTray(module);
            menu.Add(item);
        }

        menu.Add(new NativeMenuItemSeparator());
        var windowsSettings = new NativeMenuItem("Windows screensaver settings");
        windowsSettings.Click += (_, _) => MainWindow.OpenWindowsScreenSaverSettings(_viewModel);
        menu.Add(windowsSettings);

        var exit = new NativeMenuItem("Exit");
        exit.Click += (_, _) =>
        {
            if (_mainWindow is { } window)
            {
                window.HideInsteadOfClose = false;
                window.Close();
            }
            _trayIcon?.Dispose();
            desktop.Shutdown();
        };
        menu.Add(exit);
        return menu;
    }

    private void ActivateFromTray(ModuleViewModel module)
    {
        if (_viewModel is null) return;
        _viewModel.Selected = module;
        _viewModel.SetAsScreenSaver();
    }

    private void RefreshTrayMenu()
    {
        if (_trayIcon is not null &&
            ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            _trayIcon.Menu = BuildTrayMenu(desktop);
    }

    private void ShowMainWindow()
    {
        if (_mainWindow is null) return;
        _mainWindow.Show();
        if (_mainWindow.WindowState == WindowState.Minimized)
            _mainWindow.WindowState = WindowState.Normal;
        _mainWindow.Activate();
        _mainWindow.QueueRestart();
    }

    private static string? FindInstallPath(StudioSettings settings)
    {
        if (settings.InstallPath is { Length: > 0 } saved && InstallLocator.IsInstall(saved))
            return saved;

        var userModules = Path.Combine(AppPaths.UserDataDir, "modules");
        if (InstallLocator.IsInstall(userModules)) return userModules;

        var imported = Path.Combine(AppPaths.InstallDir, "modules");
        if (InstallLocator.IsInstall(imported)) return imported;

        return OperatingSystem.IsWindows() ? InstallLocator.FindFirst() : null;
    }
}
