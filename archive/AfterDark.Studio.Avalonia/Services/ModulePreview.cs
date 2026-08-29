using System.Buffers.Binary;
using System.Diagnostics;
using AfterDark.Catalog;

namespace AfterDark.Studio.Services;

/// <summary>
/// Runs a module in admhost32 and delivers its frames for the preview pane.
///
/// Frames arrive over the host's stdout rather than by embedding a native child
/// window. Hosted native windows render above the framework's own content and
/// will not clip to a scroll viewport, which is exactly what a settings page
/// needs them to do. Piping frames also means a module that dies shows up as
/// end-of-stream instead of a dead window, and nothing about the preview is
/// coupled to a window handle's lifetime.
///
/// Wire format is documented in host/admhost32.c.
/// </summary>
public sealed class ModulePreview : IDisposable
{
    private const uint Magic = 0x53464441;   // "ADFS"
    private const int  HeaderBytes = 24 + 1024;

    /// <summary>
    /// Development aid: run the Windows host through an emulator on a non-Windows
    /// build machine (set to "wine"). Null in every shipped configuration.
    /// </summary>
    public static string? Launcher { get; set; }

    private Process? _process;
    private CancellationTokenSource? _cts;
    private readonly object _gate = new();

    /// <summary>Fires off the UI thread with a BGRA frame; the buffer is reused.</summary>
    public event Action<byte[], int, int>? FrameReady;

    /// <summary>Fires when the preview stops, with a reason worth showing.</summary>
    public event Action<string>? Stopped;

    public bool IsRunning => _process is { HasExited: false };

    public void Start(AdModule module, string installPath, int[] controls, int fps,
                      int width = 640, int height = 480)
    {
        Stop();

        if (!module.CanRun)
        {
            Stopped?.Invoke("This module is 16-bit and cannot run.");
            return;
        }

        var host = AppPaths.Host;
        if (!File.Exists(host))
        {
            Stopped?.Invoke($"Renderer not found: {host}");
            return;
        }

        var psi = new ProcessStartInfo
        {
            FileName = Launcher ?? host,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = Directory.Exists(installPath)
                ? installPath
                : Path.GetDirectoryName(host)!,
        };
        if (Launcher is not null) psi.ArgumentList.Add(host);
        foreach (var a in new[]
                 {
                     installPath, module.Path, "--stream",
                     "--fps", fps.ToString(),
                     "--size", $"{width}x{height}",
                     "--frames", "0",
                     "--controls", string.Join(',', controls),
                 })
            psi.ArgumentList.Add(a);

        try
        {
            lock (_gate)
            {
                _cts = new CancellationTokenSource();
                _process = Process.Start(psi);
            }
        }
        catch (Exception ex)
        {
            Stopped?.Invoke($"Could not start the renderer: {ex.Message}");
            return;
        }

        if (_process is null) { Stopped?.Invoke("Could not start the renderer."); return; }

        var proc = _process;
        var token = _cts!.Token;
        _ = Task.Run(() => Pump(proc, token), token);
    }

    private void Pump(Process proc, CancellationToken token)
    {
        try
        {
            var stream = proc.StandardOutput.BaseStream;

            var header = new byte[HeaderBytes];
            if (!ReadHeader(stream, header, token)) return;

            int w      = (int)BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(8));
            int h      = (int)BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(12));
            int bpp    = (int)BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(16));
            int stride = (int)BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(20));

            if (w <= 0 || h <= 0 || stride <= 0 || (long)stride * h > 64 * 1024 * 1024)
            {
                Stopped?.Invoke("The renderer sent a frame size that makes no sense.");
                return;
            }

            var palette = new uint[256];
            for (int i = 0; i < 256; i++)
                palette[i] = BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(24 + i * 4));

            int frameBytes = stride * h;
            var raw  = new byte[frameBytes];
            var bgra = new byte[w * h * 4];

            while (!token.IsCancellationRequested)
            {
                if (!ReadExactly(stream, raw, frameBytes, token)) break;
                Convert(raw, bgra, w, h, stride, bpp, palette);
                FrameReady?.Invoke(bgra, w, h);
            }
        }
        catch (Exception ex) when (ex is IOException or ObjectDisposedException
                                      or InvalidOperationException or OperationCanceledException)
        {
            // Normal on Stop(): the process is gone and the pipe with it.
        }
        finally
        {
            if (!token.IsCancellationRequested)
                Stopped?.Invoke("The module stopped.");
        }
    }

    /// <summary>
    /// Find the header rather than assume it is at byte 0. The CRT or the module
    /// itself can put something on stdout before we get there; scanning for the
    /// magic keeps one stray byte from corrupting every frame that follows.
    /// </summary>
    private static bool ReadHeader(Stream s, byte[] header, CancellationToken token)
    {
        Span<byte> window = stackalloc byte[4];
        int filled = 0;

        while (!token.IsCancellationRequested)
        {
            int b = s.ReadByte();
            if (b < 0) return false;

            if (filled < 4) window[filled++] = (byte)b;
            else
            {
                window[0] = window[1]; window[1] = window[2]; window[2] = window[3];
                window[3] = (byte)b;
            }

            if (filled == 4 && BinaryPrimitives.ReadUInt32LittleEndian(window) == Magic)
            {
                window.CopyTo(header);
                return ReadExactly(s, header.AsSpan(4), header.Length - 4, token, offset: 0);
            }
        }
        return false;
    }

    private static bool ReadExactly(Stream s, byte[] buf, int count, CancellationToken token)
        => ReadExactly(s, buf.AsSpan(), count, token, 0);

    private static bool ReadExactly(Stream s, Span<byte> buf, int count,
                                    CancellationToken token, int offset)
    {
        int got = 0;
        while (got < count)
        {
            if (token.IsCancellationRequested) return false;
            int n = s.Read(buf.Slice(offset + got, count - got));
            if (n <= 0) return false;
            got += n;
        }
        return true;
    }

    private static void Convert(byte[] raw, byte[] bgra, int w, int h, int stride,
                                int bpp, uint[] palette)
    {
        if (bpp > 8)
        {
            for (int y = 0; y < h; y++)
                Buffer.BlockCopy(raw, y * stride, bgra, y * w * 4, Math.Min(w * 4, stride));
            return;
        }

        for (int y = 0; y < h; y++)
        {
            int src = y * stride, dst = y * w * 4;
            for (int x = 0; x < w; x++)
            {
                uint c = palette[raw[src + x]];
                bgra[dst + 0] = (byte)(c & 0xFF);
                bgra[dst + 1] = (byte)((c >> 8) & 0xFF);
                bgra[dst + 2] = (byte)((c >> 16) & 0xFF);
                bgra[dst + 3] = 0xFF;
                dst += 4;
            }
        }
    }

    public void Stop()
    {
        Process? p;
        CancellationTokenSource? cts;
        lock (_gate) { p = _process; cts = _cts; _process = null; _cts = null; }

        try { cts?.Cancel(); } catch (ObjectDisposedException) { }

        if (p is not null)
        {
            try
            {
                if (!p.HasExited)
                {
                    p.Kill(entireProcessTree: true);
                    p.WaitForExit(1500);
                }
            }
            catch (Exception ex) when (ex is InvalidOperationException or IOException
                                          or System.ComponentModel.Win32Exception) { }
            finally { p.Dispose(); }
        }
        cts?.Dispose();
    }

    public void Dispose() => Stop();
}
