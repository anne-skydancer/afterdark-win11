using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Forms;
using AfterDark.Catalog;
using AfterDark.Studio.Models;
using AfterDark.Studio.Services;
using AfterDark.Studio.ViewModels;
using AfterDark.Studio.Views;
using Drawing = System.Drawing;
using Forms = System.Windows.Forms;

namespace AfterDark.Studio;

public partial class App : System.Windows.Application
{
    private Forms.NotifyIcon? _trayIcon;
    private Forms.ContextMenuStrip? _trayMenu;
    private Drawing.Icon? _trayDrawingIcon;
    private MainWindow? _mainWindow;
    private MainViewModel? _viewModel;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        int screenshot = Array.IndexOf(e.Args, "--screenshot");
        if (screenshot >= 0 && screenshot + 1 < e.Args.Length)
        {
            StartScreenshot(e.Args[screenshot + 1],
                            e.Args.ElementAtOrDefault(screenshot + 2),
                            e.Args.ElementAtOrDefault(screenshot + 3));
            return;
        }

        var settings = StudioSettings.Load();
        var installPath = FindInstallPath(settings);
        settings.InstallPath = installPath;
        EnsureLegacyPreferences(installPath);

        var modules = installPath is null ? [] : ModuleCatalog.Scan(installPath);
        LabelOverrides.Load(Path.Combine(AppContext.BaseDirectory, "data", "labels.json"))
                      .Apply(modules);

        if (Environment.GetEnvironmentVariable("AD_HOST_LAUNCHER") is { Length: > 0 } launcher)
            ModulePreview.Launcher = launcher;

        _viewModel = new MainViewModel(settings, modules);
        _mainWindow = new MainWindow
        {
            DataContext = _viewModel,
            HideInsteadOfClose = true,
        };
        MainWindow = _mainWindow;

        CreateTrayIcon();
        _viewModel.CatalogChanged += RefreshTrayMenu;
        _viewModel.ScreenSaverChanged += RefreshTrayMenu;
        _mainWindow.Show();
    }

    private void StartScreenshot(string output, string? modulesDirectory, string? preselect)
    {
        var settings = new StudioSettings
        {
            InstallPath = modulesDirectory ?? "(no installation selected)",
        };
        var modules = modulesDirectory is not null && Directory.Exists(modulesDirectory)
            ? ModuleCatalog.Scan(modulesDirectory)
            : [];
        LabelOverrides.Load(Path.Combine(AppContext.BaseDirectory, "data", "labels.json"))
                      .Apply(modules);

        _viewModel = new MainViewModel(settings, modules);
        if (preselect is { Length: > 0 })
            _viewModel.Selected = _viewModel.Visible.FirstOrDefault(
                module => module.FileName.Equals(preselect, StringComparison.OrdinalIgnoreCase))
                ?? _viewModel.Selected;

        _mainWindow = new MainWindow
        {
            DataContext = _viewModel,
            Width = 1100,
            Height = 700,
        };
        MainWindow = _mainWindow;

        bool captured = false;
        _mainWindow.ContentRendered += async (_, _) =>
        {
            if (captured) return;
            captured = true;
            try
            {
                int wait = int.TryParse(Environment.GetEnvironmentVariable("AD_SHOT_WAIT"), out var seconds)
                    ? seconds : 0;
                if (wait > 0) await Task.Delay(TimeSpan.FromSeconds(wait));
                _mainWindow.CaptureToPng(output);
                Console.WriteLine($"wrote {output} ({modules.Count} modules)");
                Shutdown(0);
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine(ex.Message);
                Shutdown(1);
            }
        };
        _mainWindow.Show();
    }

    private static void EnsureLegacyPreferences(string? installPath)
    {
        if (installPath is null) return;
        try { LegacyModulePreferences.EnsureArtCriticPath(installPath); }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            // Optional compatibility preference; never block Studio startup.
        }
    }

    private void CreateTrayIcon()
    {
        if (_viewModel is null) return;

        _trayDrawingIcon = CreateTrayDrawingIcon();
        _trayIcon = new Forms.NotifyIcon
        {
            Icon = _trayDrawingIcon,
            Text = "After Dark Studio",
            Visible = true,
        };
        _trayIcon.MouseClick += (_, args) =>
        {
            if (args.Button == Forms.MouseButtons.Left)
                Dispatcher.Invoke(ShowMainWindow);
        };
        RefreshTrayMenu();
    }

    private void RefreshTrayMenu()
    {
        if (_trayIcon is null || _viewModel is null) return;

        var menu = new Forms.ContextMenuStrip();
        menu.Items.Add("Open After Dark Studio", null, (_, _) => Dispatcher.Invoke(ShowMainWindow));
        menu.Items.Add(new Forms.ToolStripSeparator());

        var runnable = _viewModel.All.Where(module => module.CanRun).ToList();
        if (runnable.Count == 0)
            menu.Items.Add(new Forms.ToolStripMenuItem("No runnable modules") { Enabled = false });

        foreach (var module in runnable)
        {
            var item = new Forms.ToolStripMenuItem(module.Title)
            {
                Checked = ReferenceEquals(module, _viewModel.Selected),
                CheckOnClick = false,
            };
            item.Click += (_, _) => Dispatcher.Invoke(() => ActivateFromTray(module));
            menu.Items.Add(item);
        }

        menu.Items.Add(new Forms.ToolStripSeparator());
        menu.Items.Add("Windows screensaver settings", null,
            (_, _) => Dispatcher.Invoke(() => Views.MainWindow.OpenWindowsScreenSaverSettings(_viewModel)));
        menu.Items.Add("Exit", null, (_, _) => Dispatcher.Invoke(ExitFromTray));

        var old = _trayMenu;
        _trayMenu = menu;
        _trayIcon.ContextMenuStrip = menu;
        old?.Dispose();
    }

    private void ActivateFromTray(ModuleViewModel module)
    {
        if (_viewModel is null) return;
        _viewModel.Selected = module;
        _viewModel.SetAsScreenSaver();
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

    private void ExitFromTray()
    {
        if (_mainWindow is not null)
        {
            _mainWindow.HideInsteadOfClose = false;
            _mainWindow.Close();
        }
        Shutdown();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        if (_trayIcon is not null) _trayIcon.Visible = false;
        _trayMenu?.Dispose();
        _trayIcon?.Dispose();
        _trayDrawingIcon?.Dispose();
        base.OnExit(e);
    }

    private static Drawing.Icon CreateTrayDrawingIcon()
    {
        const int size = 32;
        using var bitmap = new Drawing.Bitmap(size, size, Drawing.Imaging.PixelFormat.Format32bppArgb);

        void Pixel(int x, int y, byte red, byte green, byte blue) =>
            bitmap.SetPixel(x, y, Drawing.Color.FromArgb(255, red, green, blue));

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
            Pixel(x, y, rim ? (byte)0xF2 : (byte)0x1B,
                        rim ? (byte)0xB3 : (byte)0x1D,
                        rim ? (byte)0x3D : (byte)0x27);
        }

        string[] letterA = ["01110", "10001", "10001", "11111", "10001", "10001", "10001"];
        string[] letterD = ["11110", "10001", "10001", "10001", "10001", "10001", "11110"];

        void Letter(string[] rows, int left, byte red, byte green, byte blue)
        {
            for (int row = 0; row < rows.Length; row++)
            for (int column = 0; column < rows[row].Length; column++)
            {
                if (rows[row][column] != '1') continue;
                for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++)
                    Pixel(left + column * 2 + dx, 9 + row * 2 + dy, red, green, blue);
            }
        }

        Letter(letterA, 5, 0xF2, 0xB3, 0x3D);
        Letter(letterD, 17, 0xF4, 0xF4, 0xF6);

        IntPtr handle = bitmap.GetHicon();
        try
        {
            using var borrowed = Drawing.Icon.FromHandle(handle);
            return (Drawing.Icon)borrowed.Clone();
        }
        finally { DestroyIcon(handle); }
    }

    [DllImport("user32.dll")]
    private static extern bool DestroyIcon(IntPtr handle);

    private static string? FindInstallPath(StudioSettings settings)
    {
        if (settings.InstallPath is { Length: > 0 } saved && InstallLocator.IsInstall(saved))
            return saved;

        var userModules = Path.Combine(AppPaths.UserDataDir, "modules");
        if (InstallLocator.IsInstall(userModules)) return userModules;

        var imported = Path.Combine(AppPaths.InstallDir, "modules");
        if (InstallLocator.IsInstall(imported)) return imported;

        return InstallLocator.FindFirst();
    }
}