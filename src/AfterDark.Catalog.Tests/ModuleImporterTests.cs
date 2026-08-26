using AfterDark.Studio.Services;

namespace AfterDark.Catalog.Tests;

public class ModuleImporterTests
{
    [Fact]
    public void Import_preserves_generation_layout_and_leaves_source_unchanged()
    {
        var testRoot = Path.Combine(Path.GetTempPath(), $"afterdark-import-{Guid.NewGuid():N}");
        var source = Path.Combine(testRoot, "disc");
        var destination = Path.Combine(testRoot, "imported");

        try
        {
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "ADXPL510.DLL"), "engine-4");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "TOASTERS.AD"), "toasters");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "RAIN.AD"), "modern-rain");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "PICTURES", "TOASTER.BMP"), "picture");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "PICTURES", "README.TXT"), "not a picture");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "3DMINOR.MID"), "points");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "FIREBOMB.MID"), "slow burn");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "SEAPIXIE.MID"), "swirling");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "BABY.MID"), "baby toasters");
            Write(Path.Combine(source, "ADE", "FILES", "AD40", "TOASTERS.MID"), "flying toasters");
            Write(Path.Combine(source, "ADE", "FILES", "ENGINE", "STARRYNI.AD"), "starry");
            Write(Path.Combine(source, "ADE", "FILES", "CLASSIC", "ADXPL300.DLL"), "engine-3");
            Write(Path.Combine(source, "ADE", "FILES", "CLASSIC", "RAIN.AD"), "classic-rain");

            var result = ModuleImporter.Import(source, destination);

            Assert.Equal(4, result.ModuleCount);
            Assert.Equal(1, result.PictureCount);
            Assert.Equal(5, result.MusicCount);
            Assert.Equal("modern-rain", File.ReadAllText(Path.Combine(destination, "RAIN.AD")));
            Assert.Equal("classic-rain", File.ReadAllText(Path.Combine(destination, "classic", "RAIN.AD")));
            Assert.True(File.Exists(Path.Combine(destination, "ADXPL510.DLL")));
            Assert.True(File.Exists(Path.Combine(destination, "STARRYNI.AD")));
            Assert.Equal("picture", File.ReadAllText(
                Path.Combine(destination, "PICTURES", "TOASTER.BMP")));
            Assert.False(File.Exists(Path.Combine(destination, "PICTURES", "README.TXT")));
            Assert.Equal("baby toasters", File.ReadAllText(
                Path.Combine(destination, "Music", "Baby Toasters.mid")));
            Assert.Equal("flying toasters", File.ReadAllText(
                Path.Combine(destination, "Music", "Flying Toasters.mid")));
            Assert.Equal(5, Directory.GetFiles(Path.Combine(destination, "Music")).Length);
            Assert.Equal("modern-rain", File.ReadAllText(
                Path.Combine(source, "ADE", "FILES", "AD40", "RAIN.AD")));
        }
        finally
        {
            if (Directory.Exists(testRoot)) Directory.Delete(testRoot, recursive: true);
        }
    }

    [Fact]
    public void Import_rejects_a_folder_without_the_ad4_engine()
    {
        var testRoot = Path.Combine(Path.GetTempPath(), $"afterdark-import-{Guid.NewGuid():N}");
        Directory.CreateDirectory(testRoot);

        try
        {
            var error = Assert.Throws<InvalidDataException>(() =>
                ModuleImporter.Import(testRoot, Path.Combine(testRoot, "imported")));
            Assert.Contains(InstallLocator.EngineDll, error.Message);
        }
        finally
        {
            Directory.Delete(testRoot, recursive: true);
        }
    }

    [Fact]
    public void Art_Critic_preference_points_at_imported_pictures_and_preserves_custom_path()
    {
        if (!OperatingSystem.IsWindows()) return;

        var testRoot = Path.Combine(Path.GetTempPath(), $"afterdark-critic-{Guid.NewGuid():N}");
        var modules = Path.Combine(testRoot, "modules");
        var pictures = Path.Combine(modules, "PICTURES");
        var customPictures = Path.Combine(testRoot, "custom-pictures");
        var ini = Path.Combine(testRoot, "MODULES.INI");

        try
        {
            Write(Path.Combine(modules, "CRITIC.AD"), "module");
            Directory.CreateDirectory(pictures);
            Directory.CreateDirectory(customPictures);

            Assert.True(LegacyModulePreferences.EnsureArtCriticPath(modules, ini));
            Assert.Contains($"Art Path={Path.GetFullPath(pictures)}", File.ReadAllText(ini));

            File.WriteAllText(ini, $"[ArtCritic d29]\r\nArt Path={customPictures}\r\n");
            Assert.True(LegacyModulePreferences.EnsureArtCriticPath(modules, ini));
            Assert.Contains($"Art Path={customPictures}", File.ReadAllText(ini));
        }
        finally
        {
            if (Directory.Exists(testRoot)) Directory.Delete(testRoot, recursive: true);
        }
    }

    private static void Write(string path, string contents)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, contents);
    }
}