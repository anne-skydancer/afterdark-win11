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
        // Paths come from Setup's HKLM record, not from wherever a binary sits.
        if (DataContext is MainViewModel vm) vm.SetAsScreenSaver();
    }
}
