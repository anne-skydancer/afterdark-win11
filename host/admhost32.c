/*
 * admhost32 -- a minimal 32-bit host for After Dark 4 modules.
 *
 * This is the stage-2 spike from docs/DESIGN.md: prove that a custom host can
 * load a user's ADXPL510.DLL and an .AD module, drive the recovered ABI, and
 * get real pixels out. It renders into an offscreen DIB and writes a .BMP, so
 * the result is inspectable rather than a claim.
 *
 * It is deliberately diagnostic. Every step reports what it did, what the
 * module returned, and anything the module left in the status field. A crash
 * is caught by a vectored handler that names the step that faulted, because
 * "which message killed it" is the whole point of a spike.
 *
 * MUST be built as 32-bit x86. A 64-bit process cannot load these modules.
 *
 *   MinGW:  i686-w64-mingw32-gcc -O2 -o admhost32.exe admhost32.c -lgdi32 -luser32
 *   MSVC:   cl /O2 admhost32.c user32.lib gdi32.lib
 *
 * Ships no Berkeley Systems code. Loads the user's own installation at runtime.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define AD_MODULE32_NO_WINDOWS_H
#include "../include/ad_module32.h"

/* ------------------------------------------------------------------ state */

static const char *g_step = "startup";
static int         g_verbose = 1;

static void step(const char *what)
{
    g_step = what;
    if (g_verbose) printf("  [ .. ] %s\n", what);
}

static void ok(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    printf("  [ ok ] "); vprintf(fmt, ap); printf("\n");
    va_end(ap);
}

static void fail(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    printf("  [FAIL] "); vprintf(fmt, ap);
    printf("  (last error %lu)\n", (unsigned long)GetLastError());
    va_end(ap);
}

static LONG CALLBACK crash_filter(EXCEPTION_POINTERS *ep)
{
    printf("\n=========================================================\n");
    printf("  CRASH during: %s\n", g_step);
    printf("  code 0x%08lX at 0x%08lX\n",
           (unsigned long)ep->ExceptionRecord->ExceptionCode,
           (unsigned long)(ULONG_PTR)ep->ExceptionRecord->ExceptionAddress);
    printf("=========================================================\n");
    fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ------------------------------------------------------------------- BMP  */

static int save_bmp(const char *path, HBITMAP bmp, HDC dc, int w, int h, int bpp)
{
    BITMAPFILEHEADER fh;
    BITMAPINFO      *bi;
    DWORD            img, pal, hdr, wrote;
    HANDLE           fp;
    void            *bits;
    int              colors = (bpp <= 8) ? (1 << bpp) : 0;

    pal = colors * sizeof(RGBQUAD);
    hdr = sizeof(BITMAPINFOHEADER) + pal;
    img = (DWORD)(((w * bpp + 31) / 32) * 4) * (DWORD)h;

    bi = (BITMAPINFO *)calloc(1, hdr);
    if (!bi) return 0;
    bi->bmiHeader.biSize   = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth  = w;
    bi->bmiHeader.biHeight = h;
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = (WORD)bpp;
    bi->bmiHeader.biCompression = BI_RGB;

    bits = malloc(img);
    if (!bits) { free(bi); return 0; }

    if (!GetDIBits(dc, bmp, 0, h, bits, bi, DIB_RGB_COLORS)) {
        free(bits); free(bi); return 0;
    }

    ZeroMemory(&fh, sizeof(fh));
    fh.bfType    = 0x4D42; /* "BM" */
    fh.bfOffBits = sizeof(fh) + hdr;
    fh.bfSize    = fh.bfOffBits + img;

    fp = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (fp == INVALID_HANDLE_VALUE) { free(bits); free(bi); return 0; }
    WriteFile(fp, &fh, sizeof(fh), &wrote, NULL);
    WriteFile(fp, bi, hdr, &wrote, NULL);
    WriteFile(fp, bits, img, &wrote, NULL);
    CloseHandle(fp);

    free(bits); free(bi);
    return 1;
}

/* Cheap "did anything actually get drawn" check: count distinct byte values. */
static int surface_variety(const void *bits, size_t len)
{
    unsigned char seen[256];
    size_t i; int n = 0;
    ZeroMemory(seen, sizeof(seen));
    for (i = 0; i < len; i++) seen[((const unsigned char *)bits)[i]] = 1;
    for (i = 0; i < 256; i++) n += seen[i];
    return n;
}

/* ---------------------------------------------------------------- present */

static volatile int g_quit;
static int  g_mouse_armed;
static POINT g_last_mouse;

static LRESULT CALLBACK present_proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        g_quit = 1;
        return 0;
    case WM_MOUSEMOVE: {
        /* Windows delivers spurious moves; only a real displacement counts. */
        POINT p; GetCursorPos(&p);
        if (!g_mouse_armed) { g_last_mouse = p; g_mouse_armed = 1; return 0; }
        if (abs(p.x - g_last_mouse.x) > 4 || abs(p.y - g_last_mouse.y) > 4) g_quit = 1;
        return 0;
    }
    case WM_CLOSE: case WM_DESTROY:
        g_quit = 1;
        return 0;
    }
    return DefWindowProcA(w, m, wp, lp);
}

/* Integer-scale where it fits, letterboxed and centred; the pixel art is the
 * point, so a smooth stretch would be the wrong default. */
static void present_blit(HDC dst, int dw, int dh, HDC src, int sw, int sh, int integer_scale)
{
    int scale = 1, tw, th, ox, oy;
    RECT full;

    if (integer_scale) {
        int sx = dw / sw, sy = dh / sh;
        scale = sx < sy ? sx : sy;
        if (scale < 1) scale = 1;
        tw = sw * scale; th = sh * scale;
    } else {
        double ar = (double)sw / (double)sh;
        tw = dw; th = (int)(dw / ar);
        if (th > dh) { th = dh; tw = (int)(dh * ar); }
    }
    ox = (dw - tw) / 2; oy = (dh - th) / 2;

    if (ox > 0 || oy > 0) {
        HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
        full.left = 0; full.top = 0; full.right = dw; full.bottom = dh;
        FillRect(dst, &full, black);
    }
    if (tw == sw && th == sh)
        BitBlt(dst, ox, oy, tw, th, src, 0, 0, SRCCOPY);
    else {
        SetStretchBltMode(dst, integer_scale ? COLORONCOLOR : HALFTONE);
        StretchBlt(dst, ox, oy, tw, th, src, 0, 0, sw, sh, SRCCOPY);
    }
}

/* ---------------------------------------------------------------- palette */

static char g_pal_name[64];

static BOOL CALLBACK first_pal(HMODULE m, LPCSTR type, LPSTR name, LONG_PTR p)
{
    (void)m; (void)type; (void)p;
    if (IS_INTRESOURCE(name)) wsprintfA(g_pal_name, "#%u", (unsigned)(ULONG_PTR)name);
    else lstrcpynA(g_pal_name, name, sizeof(g_pal_name));
    return FALSE;   /* first one is enough */
}

/* After Dark modules carry their palette as a raw LOGPALETTE in a "PAL"
 * resource. Install it in the DC *and* in the DIB colour table, so the indices
 * the engine writes mean what the module intended. Without this you get a
 * correct image in wrong colours. */
static HPALETTE install_module_palette(HMODULE mod, HDC dc, int *out_count)
{
    HRSRC   res;
    HGLOBAL h;
    const LOGPALETTE *lp;
    HPALETTE pal;
    RGBQUAD  tbl[256];
    int      n, i;

    *out_count = 0;
    g_pal_name[0] = '\0';
    EnumResourceNamesA(mod, "PAL", first_pal, 0);
    if (!g_pal_name[0]) return NULL;

    res = FindResourceA(mod, (g_pal_name[0] == '#')
                             ? MAKEINTRESOURCEA(atoi(g_pal_name + 1))
                             : g_pal_name, "PAL");
    if (!res) return NULL;
    h = LoadResource(mod, res);
    if (!h) return NULL;
    lp = (const LOGPALETTE *)LockResource(h);
    if (!lp || lp->palVersion != 0x300) return NULL;

    n = lp->palNumEntries;
    if (n <= 0 || n > 256) return NULL;

    pal = CreatePalette(lp);
    if (!pal) return NULL;
    SelectPalette(dc, pal, FALSE);
    RealizePalette(dc);

    for (i = 0; i < n; i++) {
        tbl[i].rgbRed   = lp->palPalEntry[i].peRed;
        tbl[i].rgbGreen = lp->palPalEntry[i].peGreen;
        tbl[i].rgbBlue  = lp->palPalEntry[i].peBlue;
        tbl[i].rgbReserved = 0;
    }
    SetDIBColorTable(dc, 0, (UINT)n, tbl);

    *out_count = n;
    return pal;
}

/* ------------------------------------------------------------------- main */

static const char *msg_name(DWORD m)
{
    switch (m) {
    case AD_MSG_MODULESELECTED:   return "MODULESELECTED";
    case AD_MSG_MODULEDESELECTED: return "MODULEDESELECTED";
    case AD_MSG_PREINITIALIZE:    return "PREINITIALIZE";
    case AD_MSG_BLANK:            return "BLANK";
    case AD_MSG_DRAWFRAME:        return "DRAWFRAME";
    case AD_MSG_CLOSE:            return "CLOSE";
    case AD_MSG_PAINT:            return "PAINT";
    default:                      return "msg";
    }
}

static AD_MODULEPROC g_proc;
static AD_MODULE32   g_block;

static int send_msg(DWORD m, DWORD param)
{
    int r;
    char label[64];
    wsprintfA(label, "Module(%s=%lu)", msg_name(m), (unsigned long)m);
    step(label);

    g_block.dwMessage = m;
    g_block.dwParam   = param;
    r = g_proc(&g_block);

    if (g_block.szMessage[0])
        printf("         module says: \"%s\"\n", g_block.szMessage);
    if (r == AD_OK)
        ok("%s -> 0 (AD_OK)", label);
    else if (r == AD_RESTART_ME)
        ok("%s -> 3 (AD_RESTART_ME)", label);
    else
        printf("  [ ?? ] %s -> %d\n", label, r);
    return r;
}

static void usage(void)
{
    printf(
"admhost32 -- minimal host for After Dark 4 modules\n"
"\n"
"usage: admhost32 <install-dir> <module.AD> [options]\n"
"\n"
"  <install-dir>   folder containing ADXPL510.DLL (your After Dark install)\n"
"  <module.AD>     module to run; bare name is looked up in <install-dir>\n"
"\n"
"options:\n"
"  --frames N      DRAWFRAME iterations (default 60)\n"
"  --size WxH      surface size (default 640x480)\n"
"  --bpp 8|32      surface depth (default 8, as modules expect)\n"
"  --controls a,b,c,d   iControlValue[0..3] (default 0,0,0,0)\n"
"  --bmp FILE      write the final surface (default admhost32.bmp)\n"
"  --fps N         frame pacing; 0 = unpaced like the original (default 30)\n"
"  --present       show a window and run until input (screensaver mode)\n"
"  --parent HWND   render inside an existing window (the .scr's preview or\n"
"                  full-screen surface); implies --present\n"
"  --scale MODE    integer (default) or stretch\n"
"  --quiet\n");
}

int main(int argc, char **argv)
{
    char install[MAX_PATH], modpath[MAX_PATH], engine[MAX_PATH];
    const char *bmp = "admhost32.bmp";
    int  frames = 60, w = 640, h = 480, bpp = 8, fps = 30;
    int  present = 0, integer_scale = 1;
    HWND parent = NULL;
    int  ctl[4] = {0,0,0,0};
    int  i, r, restarts = 0;
    HMODULE hEngine, hMod;
    HWND    wnd;
    HDC     screen, mem;
    HBITMAP dib, old;
    HPALETTE pal = NULL;
    void   *bits = NULL;
    BITMAPINFO *bi;
    DWORD   stride, imgbytes;

    if (argc < 3) { usage(); return 2; }

    lstrcpynA(install, argv[1], MAX_PATH);
    lstrcpynA(modpath, argv[2], MAX_PATH);

    for (i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i+1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--bmp") && i+1 < argc) bmp = argv[++i];
        else if (!strcmp(argv[i], "--bpp") && i+1 < argc) bpp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fps") && i+1 < argc) fps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i+1 < argc) sscanf(argv[++i], "%dx%d", &w, &h);
        else if (!strcmp(argv[i], "--controls") && i+1 < argc)
            sscanf(argv[++i], "%d,%d,%d,%d", &ctl[0], &ctl[1], &ctl[2], &ctl[3]);
        else if (!strcmp(argv[i], "--present")) present = 1;
        else if (!strcmp(argv[i], "--parent") && i+1 < argc)
            { parent = (HWND)(ULONG_PTR)strtoul(argv[++i], NULL, 0); present = 1; }
        else if (!strcmp(argv[i], "--scale") && i+1 < argc)
            integer_scale = strcmp(argv[++i], "stretch") != 0;
        else if (!strcmp(argv[i], "--quiet")) g_verbose = 0;
        else { printf("unknown option: %s\n", argv[i]); usage(); return 2; }
    }

    if (sizeof(AD_MODULE32) != AD_MODULE32_SIZE) {
        printf("FATAL: AD_MODULE32 is %d bytes, must be %d. "
               "Are you building 32-bit?\n",
               (int)sizeof(AD_MODULE32), AD_MODULE32_SIZE);
        return 3;
    }
    if (sizeof(void *) != 4) {
        printf("FATAL: this is a %d-bit build. After Dark modules are 32-bit "
               "and cannot be loaded by a 64-bit process.\n",
               (int)(sizeof(void *) * 8));
        return 3;
    }

    AddVectoredExceptionHandler(1, crash_filter);

    printf("admhost32 -- After Dark 4 module host (spike)\n");
    printf("  surface %dx%d @ %d bpp, %d frames, fps %d\n\n", w, h, bpp, frames, fps);

    /* -- 1. the engine, from the user's own install -- */
    wsprintfA(engine, "%s\\%s", install, AD_ENGINE_DLL);
    step("SetCurrentDirectory + SetDllDirectory to the install");
    SetCurrentDirectoryA(install);
    SetDllDirectoryA(install);

    step("LoadLibrary ADXPL510.DLL");
    hEngine = LoadLibraryA(engine);
    if (!hEngine) {
        fail("could not load %s", engine);
        printf("\n  The engine must load before any module: modules import\n"
               "  150-300 functions from it and will not bind without it.\n");
        return 1;
    }
    ok("engine at 0x%08lX", (unsigned long)(ULONG_PTR)hEngine);

    /* -- 2. the module -- */
    if (!strchr(modpath, '\\') && !strchr(modpath, '/')) {
        char tmp[MAX_PATH];
        wsprintfA(tmp, "%s\\%s", install, modpath);
        lstrcpynA(modpath, tmp, MAX_PATH);
    }
    step("LoadLibrary the module");
    hMod = LoadLibraryA(modpath);
    if (!hMod) { fail("could not load %s", modpath); return 1; }
    ok("module at 0x%08lX", (unsigned long)(ULONG_PTR)hMod);

    /* -- 3. the entry point: decorated first, then undecorated -- */
    step("GetProcAddress " AD_ENTRY_DECORATED " / " AD_ENTRY_UNDECORATED);
    g_proc = (AD_MODULEPROC)GetProcAddress(hMod, AD_ENTRY_DECORATED);
    if (g_proc) ok("resolved %s (MSVC-decorated)", AD_ENTRY_DECORATED);
    else {
        g_proc = (AD_MODULEPROC)GetProcAddress(hMod, AD_ENTRY_UNDECORATED);
        if (g_proc) ok("resolved %s (undecorated)", AD_ENTRY_UNDECORATED);
    }
    if (!g_proc) { fail("no Module entry point -- is this an After Dark module?"); return 1; }

    /* -- 4. an offscreen surface and its DC: this is the module's hDC -- */
    step("create DIB section + memory DC");
    screen = GetDC(NULL);
    mem    = CreateCompatibleDC(screen);

    bi = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    bi->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth    = w;
    bi->bmiHeader.biHeight   = -h;              /* top-down */
    bi->bmiHeader.biPlanes   = 1;
    bi->bmiHeader.biBitCount = (WORD)bpp;
    bi->bmiHeader.biCompression = BI_RGB;
    if (bpp <= 8) {
        /* A plain grey ramp; the module/engine will install its own palette. */
        int c;
        bi->bmiHeader.biClrUsed = 256;
        for (c = 0; c < 256; c++) {
            bi->bmiColors[c].rgbRed   = (BYTE)c;
            bi->bmiColors[c].rgbGreen = (BYTE)c;
            bi->bmiColors[c].rgbBlue  = (BYTE)c;
        }
    }
    dib = CreateDIBSection(mem, bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib) { fail("CreateDIBSection"); return 1; }
    old = (HBITMAP)SelectObject(mem, dib);

    /* deferred: the module's palette is installed once the module is loaded */
    stride   = (DWORD)(((w * bpp + 31) / 32) * 4);
    imgbytes = stride * (DWORD)h;
    ok("surface ready, %lu bytes, bits at 0x%08lX",
       (unsigned long)imgbytes, (unsigned long)(ULONG_PTR)bits);

    /* -- 5. the window. In present mode this is what the user sees; otherwise
     *       a hidden one, because modules may ask the HWND about geometry. -- */
    if (present) {
        WNDCLASSA wc;
        RECT pr;
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc   = present_proc;
        wc.hInstance     = GetModuleHandle(NULL);
        wc.hCursor       = NULL;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "admhost32_present";
        RegisterClassA(&wc);

        if (parent && IsWindow(parent)) {
            step("create child window inside the supplied parent");
            GetClientRect(parent, &pr);
            wnd = CreateWindowExA(0, "admhost32_present", NULL,
                                  WS_CHILD | WS_VISIBLE,
                                  0, 0, pr.right, pr.bottom,
                                  parent, NULL, GetModuleHandle(NULL), NULL);
        } else {
            step("create full-screen window");
            wnd = CreateWindowExA(WS_EX_TOPMOST, "admhost32_present", "After Dark",
                                  WS_POPUP | WS_VISIBLE,
                                  0, 0, GetSystemMetrics(SM_CXSCREEN),
                                  GetSystemMetrics(SM_CYSCREEN),
                                  NULL, NULL, GetModuleHandle(NULL), NULL);
            if (wnd) ShowCursor(FALSE);
        }
        if (!wnd) { fail("could not create the presentation window"); return 1; }
        SetFocus(wnd);
    } else {
        step("create hidden window");
        wnd = CreateWindowExA(0, "STATIC", "admhost32", WS_POPUP,
                              0, 0, w, h, NULL, NULL, GetModuleHandle(NULL), NULL);
        if (!wnd) fail("CreateWindow (continuing with NULL hWnd)");
    }

    /* -- 6. fill the block -- */
    step("populate AD_MODULE32");
    ZeroMemory(&g_block, sizeof(g_block));
    g_block.cbSize    = AD_MODULE32_SIZE;
    g_block.dwFlags   = (bpp <= 8) ? AD_FLAG_PALETTE : 0;
    g_block.hWnd      = wnd;
    g_block.hModule   = hMod;
    g_block.hDC       = mem;                  /* the module draws here */
    SetRect(&g_block.rcClient, 0, 0, w, h);
    g_block.rcDemo = g_block.rcClient;
    for (i = 0; i < 4; i++) g_block.iControlValue[i] = ctl[i];
    ok("cbSize=0x%lX hDC=0x%08lX controls={%d,%d,%d,%d}",
       (unsigned long)g_block.cbSize, (unsigned long)(ULONG_PTR)g_block.hDC,
       ctl[0], ctl[1], ctl[2], ctl[3]);

    /* -- 6b. the module's own palette -- */
    if (bpp <= 8) {
        int pn = 0;
        step("install the module's PAL resource");
        pal = install_module_palette(hMod, mem, &pn);
        if (pal) ok("installed %d-entry palette from PAL resource \"%s\"", pn, g_pal_name);
        else {
            pal = CreateHalftonePalette(mem);
            if (pal) { SelectPalette(mem, pal, FALSE); RealizePalette(mem); }
            printf("  [ ?? ] no usable PAL resource; falling back to a halftone palette\n");
        }
    }

    /* -- 7. drive the lifecycle -- */
    printf("\n--- lifecycle ---\n");
    if (send_msg(AD_MSG_MODULESELECTED, 0) != AD_OK)
        printf("  (module declined selection; continuing anyway)\n");

restart:
    if (send_msg(AD_MSG_PREINITIALIZE, 0) != AD_OK)
        printf("  (PREINITIALIZE non-zero)\n");
    send_msg(AD_MSG_BLANK, 0);

    if (present) {
        /* Run until the user does something. This is the screensaver loop:
         * the module never learns about pacing -- that is entirely ours. */
        DWORD period = (fps > 0) ? (DWORD)(1000 / fps) : 0;
        HDC   wdc = GetDC(wnd);
        RECT  rc;
        MSG   msg;

        g_verbose = 0;
        while (!g_quit) {
            DWORD t0 = GetTickCount();

            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) { g_quit = 1; break; }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            if (g_quit) break;

            g_block.dwMessage = AD_MSG_DRAWFRAME;
            g_block.dwParam   = 0;
            g_step = "Module(DRAWFRAME)";
            r = g_proc(&g_block);
            if (r == AD_RESTART_ME) {
                g_block.dwMessage = AD_MSG_PREINITIALIZE; g_proc(&g_block);
                g_block.dwMessage = AD_MSG_BLANK;         g_proc(&g_block);
            } else if (r != AD_OK) break;

            GetClientRect(wnd, &rc);
            present_blit(wdc, rc.right, rc.bottom, mem, w, h, integer_scale);

            if (period) {
                DWORD dt = GetTickCount() - t0;
                if (dt < period) Sleep(period - dt);
            }
        }
        ReleaseDC(wnd, wdc);
        if (!parent) ShowCursor(TRUE);
        g_verbose = 1;
        goto teardown;
    }

    printf("\n--- %d frames ---\n", frames);
    {
        DWORD period = (fps > 0) ? (DWORD)(1000 / fps) : 0;
        int   drawn  = 0;
        g_verbose = 0;
        for (i = 0; i < frames; i++) {
            DWORD t0 = GetTickCount();
            g_block.dwMessage = AD_MSG_DRAWFRAME;
            g_block.dwParam   = 0;
            g_step = "Module(DRAWFRAME)";
            r = g_proc(&g_block);
            drawn++;
            if (r == AD_RESTART_ME) {
                printf("  frame %d: module asked to restart\n", i);
                if (++restarts > 3) { printf("  too many restarts, stopping\n"); break; }
                g_verbose = 1;
                goto restart;
            }
            if (r != AD_OK) {
                printf("  frame %d: returned %d%s%s\n", i, r,
                       g_block.szMessage[0] ? " -- " : "",
                       g_block.szMessage[0] ? g_block.szMessage : "");
                break;
            }
            if (period) {
                DWORD dt = GetTickCount() - t0;
                if (dt < period) Sleep(period - dt);
            }
        }
        g_verbose = 1;
        ok("%d DRAWFRAME calls completed", drawn);
    }

    send_msg(AD_MSG_PAINT, 0);

    /* -- 8. did anything actually get drawn? -- */
    printf("\n--- result ---\n");
    {
        int variety = surface_variety(bits, imgbytes);
        printf("  distinct byte values on the surface: %d\n", variety);
        if (variety <= 1)
            printf("  [ ?? ] surface is uniform -- the module drew nothing we can see\n");
        else
            ok("surface has content");
    }
    step("write BMP");
    if (save_bmp(bmp, dib, mem, w, h, bpp)) ok("wrote %s", bmp);
    else fail("could not write %s", bmp);

    /* -- 9. teardown, in the host's order -- */
teardown:
    printf("\n--- teardown ---\n");
    send_msg(AD_MSG_CLOSE, 0);
    send_msg(AD_MSG_MODULEDESELECTED, 0);

    step("cleanup");
    if (pal) DeleteObject(pal);
    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    if (wnd) DestroyWindow(wnd);
    free(bi);
    FreeLibrary(hMod);
    FreeLibrary(hEngine);

    printf("\ndone.\n");
    return 0;
}
