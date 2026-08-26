using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using AfterDark.Studio.ViewModels;

namespace AfterDark.Studio.Views;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        AvaloniaXamlLoader.Load(this);
        if (this.FindControl<Button>("SetSaver") is { } b)
            b.Click += OnSetScreenSaver;
    }

    private void OnSetScreenSaver(object? sender, RoutedEventArgs e)
    {
        if (DataContext is not MainViewModel vm) return;

        // The .scr and Studio ship side by side; the installer keeps them there.
        var dir = AppContext.BaseDirectory;
        vm.SetAsScreenSaver(
            Path.Combine(dir, "AfterDarkModern.scr"),
            Path.Combine(dir, "AfterDark.Studio.exe"));
    }
}
