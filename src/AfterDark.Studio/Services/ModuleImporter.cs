namespace AfterDark.Studio.Services;

public sealed record ModuleImportResult(
    string Destination, int ModuleCount, int PictureCount, int MusicCount);

/// <summary>Imports modules from owned media without modifying the source.</summary>
public static class ModuleImporter
{
    private const int MaxDepth = 5;

    public static bool ContainsAfterDark4(string root) =>
        FindDirectoryContaining(root, InstallLocator.EngineDll) is not null;

    public static ModuleImportResult Import(string sourceRoot, string? destination = null)
    {
        var ad4 = FindDirectoryContaining(sourceRoot, InstallLocator.EngineDll)
            ?? throw new InvalidDataException(
                $"No After Dark 4 engine ({InstallLocator.EngineDll}) was found under {sourceRoot}.");

        destination ??= Path.Combine(AppPaths.UserDataDir, "modules");
        Directory.CreateDirectory(destination);

        int count = CopyFiles(ad4, destination, "*.AD");
        CopyFiles(ad4, destination, "ADXPL*.DLL");
        int pictureCount = CopyPictures(Path.Combine(ad4, "PICTURES"),
                        Path.Combine(destination, "PICTURES"));
        int musicCount = CopyMusic(ad4, Path.Combine(destination, "Music"));

        if (FindDirectoryContaining(sourceRoot, "STARRYNI.AD") is { } engine &&
            !Path.GetFullPath(engine).Equals(Path.GetFullPath(ad4), StringComparison.OrdinalIgnoreCase))
        {
            count += CopyFiles(engine, destination, "STARRYNI.AD");
        }

        if (FindDirectoryContaining(sourceRoot, "ADXPL300.DLL") is { } classic)
        {
            var classicDestination = Path.Combine(destination, "classic");
            count += CopyFiles(classic, classicDestination, "*.AD");
            CopyFiles(classic, classicDestination, "ADXPL*.DLL");
        }

        if (count == 0)
            throw new InvalidDataException("The After Dark engine was found, but no modules were available to import.");

        return new ModuleImportResult(destination, count, pictureCount, musicCount);
    }

    private static string? FindDirectoryContaining(string root, string fileName)
    {
        if (!Directory.Exists(root)) return null;

        var pending = new Queue<(string Path, int Depth)>();
        pending.Enqueue((root, 0));
        while (pending.TryDequeue(out var current))
        {
            try
            {
                if (File.Exists(Path.Combine(current.Path, fileName))) return current.Path;
                if (current.Depth >= MaxDepth) continue;
                foreach (var directory in Directory.EnumerateDirectories(current.Path))
                    pending.Enqueue((directory, current.Depth + 1));
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                // Skip unreadable branches while continuing to search the media.
            }
        }
        return null;
    }

    private static int CopyFiles(string source, string destination, string pattern)
    {
        Directory.CreateDirectory(destination);
        int count = 0;
        foreach (var file in Directory.EnumerateFiles(source, pattern, SearchOption.TopDirectoryOnly))
        {
            File.Copy(file, Path.Combine(destination, Path.GetFileName(file)), overwrite: true);
            count++;
        }
        return count;
    }

    private static int CopyPictures(string source, string destination)
    {
        if (!Directory.Exists(source)) return 0;

        var extensions = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            ".bmp", ".gif", ".jpg", ".jpeg",
        };
        Directory.CreateDirectory(destination);

        int count = 0;
        foreach (var file in Directory.EnumerateFiles(source, "*", SearchOption.TopDirectoryOnly))
        {
            if (!extensions.Contains(Path.GetExtension(file))) continue;
            File.Copy(file, Path.Combine(destination, Path.GetFileName(file)), overwrite: true);
            count++;
        }
        return count;
    }

    private static int CopyMusic(string source, string destination)
    {
        var tracks = new (string Source, string Destination)[]
        {
            ("3DMINOR.MID", "3DMinor.MID"),
            ("FIREBOMB.MID", "FIREBOMB.MID"),
            ("SEAPIXIE.MID", "SEAPIXIE.mid"),
            ("BABY.MID", "Baby Toasters.mid"),
            ("TOASTERS.MID", "Flying Toasters.mid"),
        };

        int count = 0;
        foreach (var track in tracks)
        {
            var sourcePath = Path.Combine(source, track.Source);
            if (!File.Exists(sourcePath)) continue;
            Directory.CreateDirectory(destination);
            File.Copy(sourcePath, Path.Combine(destination, track.Destination), overwrite: true);
            count++;
        }
        return count;
    }
}