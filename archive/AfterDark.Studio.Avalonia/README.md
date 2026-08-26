# Archived Avalonia Studio

This directory preserves the complete Avalonia 11.3.20 / .NET 10 Studio shell
as it existed when the active Windows UI moved to WPF.

It is retained for historical comparison and can be built independently:

```powershell
dotnet build archive\AfterDark.Studio.Avalonia\AfterDark.Studio.csproj
```

The archive is not referenced by the active application, installer, or tests.
New UI development belongs in `src/AfterDark.Studio`.