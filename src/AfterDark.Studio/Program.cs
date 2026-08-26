using Avalonia;
using Avalonia.Headless;
using AfterDark.Catalog;
using AfterDark.Studio.Models;
using AfterDark.Studio.ViewModels;
using AfterDark.Studio.Views;

namespace AfterDark.Studio;

internal static class Program
{
    // Avalonia needs this before any SynchronizationContext exists.
    [STAThread]
    public static int Main(string[] args)
    {
        // Development aid: render the window to a PNG without a display, so the
        // UI can be reviewed on a build machine. Not part of the shipped flow.
        int shot = Array.IndexOf(args, "--screenshot");
        if (shot >= 0 && shot + 1 < args.Length)
            return Screenshot(args[shot + 1], args.ElementAtOrDefault(shot + 2), args.ElementAtOrDefault(shot + 3));

        // Launched as "<studio> --configure" from the .scr's /c handler.
        return BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    private static int Screenshot(string outPath, string? modulesDir, string? preselect)
    {
        AppBuilder.Configure<App>()
                  .UseSkia()
                  .WithInterFont()
                  .UseHeadless(new AvaloniaHeadlessPlatformOptions { UseHeadlessDrawing = false })
                  .SetupWithoutStarting();

        var settings = new StudioSettings { InstallPath = modulesDir ?? "(no installation selected)" };
        var modules = modulesDir is not null && Directory.Exists(modulesDir)
            ? ModuleCatalog.Scan(modulesDir)
            : [];
        LabelOverrides.Load(Path.Combine(AppContext.BaseDirectory, "data", "labels.json"))
                      .Apply(modules);

        var vm = new MainViewModel(settings, modules);
        // Optional 4th arg: pre-select a module by file name, for review shots.
        if (preselect is { Length: > 0 } pick)
            vm.Selected = vm.Visible.FirstOrDefault(
                m => m.FileName.Equals(pick, StringComparison.OrdinalIgnoreCase)) ?? vm.Selected;

        var window = new MainWindow
        {
            DataContext = vm,
            Width = 1100,
            Height = 700,
        };
        window.Show();

        // Let layout and bindings settle before capturing.
        for (int i = 0; i < 12; i++)
            Avalonia.Threading.Dispatcher.UIThread.RunJobs();

        var frame = window.CaptureRenderedFrame();
        if (frame is null) { Console.Error.WriteLine("capture failed"); return 1; }
        frame.Save(outPath);
        Console.WriteLine($"wrote {outPath} ({modules.Count} modules)");
        return 0;
    }

    public static AppBuilder BuildAvaloniaApp() =>
        AppBuilder.Configure<App>()
                  .UsePlatformDetect()
                  .WithInterFont()
                  .LogToTrace();
}
