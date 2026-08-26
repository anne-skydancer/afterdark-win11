/*
 * AfterDarkModern.scr -- the Windows screensaver front end.
 *
 * Built 64-bit. It never loads a module itself, so nothing forces it to x86:
 * it creates one full-screen window per monitor and spawns a 32-bit
 * admhost32.exe to render into each. HWNDs are valid across the bitness
 * boundary, so a 64-bit parent and a 32-bit child share a window fine.
 *
 * Implements the standard, unchanged Windows contract:
 *      /s              run full screen
 *      /p <hwnd>       render the little preview in the settings dialog
 *      /c[:<hwnd>]     show configuration (launches After Dark Studio)
 *
 * Configuration comes from a small key=value file written by Studio, so the
 * .scr needs no JSON parser and starts instantly. Two are consulted, in order:
 *
 *      %LOCALAPPDATA%\AfterDarkStudio\saver.cfg     this user's choice
 *      %ProgramData%\AfterDarkStudio\saver.cfg      the machine-wide default
 *
 * so a system-wide install can give every user a working screensaver without
 * each of them configuring one.
 *
 * The install directory comes from HKLM, NOT from wherever this .scr happens to
 * sit: a system-wide install may copy it into System32 so it appears in every
 * user's Screen Saver dropdown, and admhost32.exe is not in System32.
 *
 *   x86_64-w64-mingw32-gcc -O2 -o AfterDarkModern.scr afterdark_modern.c \
 *       -lgdi32 -luser32 -lshell32 -ladvapi32 -mwindows
 */

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AD_MAX_MONITORS 8

typedef struct {
    char install[MAX_PATH];
    char module[MAX_PATH];
    char studio[MAX_PATH];
    char controls[64];
    char scale[16];
    int  fps;
    int  bpp;
    int  width, height;
} Config;

static Config      g_cfg;
static HWND        g_windows[AD_MAX_MONITORS];
static HANDLE      g_children[AD_MAX_MONITORS];
static int         g_fallback_used[AD_MAX_MONITORS];
static int         g_count;
static volatile int g_quit;
static POINT       g_origin;
static int         g_origin_set;

/* ------------------------------------------------------------------ config */

static void config_defaults(Config *c)
{
    ZeroMemory(c, sizeof(*c));
    c->fps = 30;
    c->bpp = 8;
    c->width = 640;
    c->height = 480;
    lstrcpynA(c->controls, "0,0,0,0", sizeof(c->controls));
    lstrcpynA(c->scale, "integer", sizeof(c->scale));
}

static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && (e[-1] == '\r' || e[-1] == '\n' || e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
    while (*s == ' ' || *s == '\t') memmove(s, s + 1, strlen(s));
}

/* CSIDL_LOCAL_APPDATA for this user, or CSIDL_COMMON_APPDATA for the machine. */
static void config_path(char *out, size_t n, int common)
{
    char base[MAX_PATH];
    int  folder = common ? CSIDL_COMMON_APPDATA : CSIDL_LOCAL_APPDATA;
    if (SUCCEEDED(SHGetFolderPathA(NULL, folder, NULL, 0, base)))
        _snprintf(out, n, "%s\\AfterDarkStudio\\saver.cfg", base);
    else
        lstrcpynA(out, "saver.cfg", (int)n);
}

static int config_read_file(Config *c, const char *path)
{
    char line[512];
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    while (fgets(line, sizeof(line), fp)) {
        char *eq;
        trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        trim(line); trim(eq + 1);

        if      (!_stricmp(line, "install"))  lstrcpynA(c->install, eq + 1, MAX_PATH);
        else if (!_stricmp(line, "module"))   lstrcpynA(c->module, eq + 1, MAX_PATH);
        else if (!_stricmp(line, "studio"))   lstrcpynA(c->studio, eq + 1, MAX_PATH);
        else if (!_stricmp(line, "controls")) lstrcpynA(c->controls, eq + 1, sizeof(c->controls));
        else if (!_stricmp(line, "scale"))    lstrcpynA(c->scale, eq + 1, sizeof(c->scale));
        else if (!_stricmp(line, "fps"))      c->fps = atoi(eq + 1);
        else if (!_stricmp(line, "bpp"))      c->bpp = atoi(eq + 1);
        else if (!_stricmp(line, "width"))    c->width = atoi(eq + 1);
        else if (!_stricmp(line, "height"))   c->height = atoi(eq + 1);
    }
    fclose(fp);
    return 1;
}

/* This user's settings win; the machine-wide default is the fallback. */
static int config_load(Config *c)
{
    char path[MAX_PATH];

    config_defaults(c);

    config_path(path, sizeof(path), 0);
    if (!config_read_file(c, path)) {
        config_path(path, sizeof(path), 1);
        config_read_file(c, path);
    }
    return c->install[0] && c->module[0];
}

/* Sibling files, relative to this .scr. Only a fallback -- see install_file. */
static void beside_me(const char *leaf, char *out, size_t n)
{
    char self[MAX_PATH], *slash;
    GetModuleFileNameA(NULL, self, MAX_PATH);
    slash = strrchr(self, '\\');
    if (slash) *slash = '\0';
    _snprintf(out, n, "%s\\%s", self, leaf);
}

/* Where Setup put the program. Recorded in HKLM by the installer, because a
 * system-wide install may place this .scr in System32 while admhost32.exe and
 * the shell stay in Program Files. */
static int install_dir(char *out, size_t n)
{
    HKEY  k;
    DWORD type = 0, cb = (DWORD)n;
    LONG  rc;

    rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\AfterDarkStudio", 0,
                       KEY_READ, &k);
    if (rc != ERROR_SUCCESS)
        rc = RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\AfterDarkStudio", 0,
                           KEY_READ, &k);
    if (rc != ERROR_SUCCESS) return 0;

    rc = RegQueryValueExA(k, "InstallDir", NULL, &type, (LPBYTE)out, &cb);
    RegCloseKey(k);
    if (rc != ERROR_SUCCESS || type != REG_SZ || out[0] == '\0') return 0;
    out[n - 1] = '\0';
    return 1;
}

/* A file that ships with the program: use a valid sibling for a portable or
 * staged copy. The System32 copy has no siblings, so it falls through to the
 * install directory recorded by Setup. */
static void install_file(const char *leaf, char *out, size_t n)
{
    char dir[MAX_PATH], candidate[MAX_PATH];

    beside_me(leaf, candidate, sizeof(candidate));
    if (GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES) {
        lstrcpynA(out, candidate, (int)n);
        return;
    }

    if (install_dir(dir, sizeof(dir))) {
        size_t len = strlen(dir);
        while (len > 0 && dir[len - 1] == '\\') dir[--len] = '\0';
        _snprintf(candidate, sizeof(candidate), "%s\\%s", dir, leaf);
        if (GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES) {
            lstrcpynA(out, candidate, (int)n);
            return;
        }
    }
    beside_me(leaf, out, n);
}

/* ------------------------------------------------------------------- child */

static HANDLE spawn_host_for(HWND parent, const char *module, const char *controls)
{
    char exe[MAX_PATH], cmd[2048];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    install_file("admhost32.exe", exe, sizeof(exe));

    _snprintf(cmd, sizeof(cmd),
              "\"%s\" \"%s\" \"%s\" --parent %llu --fps %d --scale %s "
              "--bpp %d --size %dx%d --controls %s --quiet",
              exe, g_cfg.install, module,
              (unsigned long long)(ULONG_PTR)parent,
              g_cfg.fps, g_cfg.scale, g_cfg.bpp,
              g_cfg.width, g_cfg.height, controls);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, g_cfg.install, &si, &pi))
        return NULL;

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

static HANDLE spawn_host(HWND parent)
{
    return spawn_host_for(parent, g_cfg.module, g_cfg.controls);
}

static HANDLE spawn_fallback(HWND parent)
{
    static const char *safe[] = { "TOASTERS.AD", "STARRYNI.AD" };
    char candidate[MAX_PATH];
    int i;

    for (i = 0; i < (int)(sizeof(safe) / sizeof(safe[0])); i++) {
        _snprintf(candidate, sizeof(candidate), "%s\\%s", g_cfg.install, safe[i]);
        if (!_stricmp(candidate, g_cfg.module)) continue;
        if (GetFileAttributesA(candidate) == INVALID_FILE_ATTRIBUTES) continue;
        return spawn_host_for(parent, candidate, "0,0,0,0");
    }
    return NULL;
}

static void stop_children(void)
{
    int i;
    for (i = 0; i < g_count; i++) {
        if (!g_children[i]) continue;
        /* The child owns 30-year-old code; ask nicely, then insist. */
        PostMessageA(g_windows[i], WM_CLOSE, 0, 0);
        if (WaitForSingleObject(g_children[i], 700) != WAIT_OBJECT_0)
            TerminateProcess(g_children[i], 0);
        CloseHandle(g_children[i]);
        g_children[i] = NULL;
    }
}

/* ------------------------------------------------------------------ window */

static LRESULT CALLBACK saver_proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        g_quit = 1;
        return 0;

    case WM_MOUSEMOVE: {
        POINT p;
        GetCursorPos(&p);
        if (!g_origin_set) { g_origin = p; g_origin_set = 1; return 0; }
        /* Windows generates spurious moves at startup; require real travel. */
        if (abs(p.x - g_origin.x) > 6 || abs(p.y - g_origin.y) > 6) g_quit = 1;
        return 0;
    }

    case WM_SETCURSOR:
        SetCursor(NULL);
        return TRUE;

    case WM_DESTROY:
        g_quit = 1;
        return 0;
    }
    return DefWindowProcA(w, m, wp, lp);
}

static BOOL CALLBACK on_monitor(HMONITOR mon, HDC dc, LPRECT rc, LPARAM p)
{
    MONITORINFO mi;
    HWND w;
    (void)dc; (void)rc; (void)p;

    if (g_count >= AD_MAX_MONITORS) return FALSE;
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoA(mon, &mi)) return TRUE;

    w = CreateWindowExA(WS_EX_TOPMOST, "AfterDarkModernSaver", "After Dark",
                        WS_POPUP | WS_VISIBLE,
                        mi.rcMonitor.left, mi.rcMonitor.top,
                        mi.rcMonitor.right - mi.rcMonitor.left,
                        mi.rcMonitor.bottom - mi.rcMonitor.top,
                        NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!w) return TRUE;

    g_windows[g_count] = w;
    g_children[g_count] = spawn_host(w);
    g_fallback_used[g_count] = 0;
    g_count++;
    return TRUE;
}

static void register_class(void)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = saver_proc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "AfterDarkModernSaver";
    RegisterClassA(&wc);
}

static int pump_until_quit(void)
{
    MSG msg;
    while (!g_quit) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_quit = 1; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (g_quit) break;

        /* If every renderer has died there is nothing left to show. */
        {
            int i, alive = 0;
            for (i = 0; i < g_count; i++) {
                if (!g_children[i]) continue;
                if (WaitForSingleObject(g_children[i], 0) == WAIT_TIMEOUT) {
                    alive++;
                    continue;
                }

                CloseHandle(g_children[i]);
                g_children[i] = NULL;
                if (!g_fallback_used[i]) {
                    g_fallback_used[i] = 1;
                    g_children[i] = spawn_fallback(g_windows[i]);
                    if (g_children[i]) alive++;
                }
            }
            if (g_count > 0 && alive == 0) break;
        }
        Sleep(30);
    }
    return 0;
}

/* -------------------------------------------------------------------- main */

static int run_fullscreen(void)
{
    int i;
    if (!config_load(&g_cfg)) return 1;   /* nothing configured: fall back to blank */
    register_class();
    EnumDisplayMonitors(NULL, NULL, on_monitor, 0);
    if (g_count == 0) return 1;

    SetCursor(NULL);
    pump_until_quit();
    stop_children();
    for (i = 0; i < g_count; i++) if (g_windows[i]) DestroyWindow(g_windows[i]);
    return 0;
}

static int run_preview(HWND parent)
{
    HANDLE child;
    int fallback_used = 0;
    if (!IsWindow(parent)) return 1;
    if (!config_load(&g_cfg)) return 1;

    child = spawn_host(parent);
    if (!child) return 1;

    /* Live only as long as the settings dialog keeps the preview window. */
    while (IsWindow(parent)) {
        if (WaitForSingleObject(child, 120) != WAIT_OBJECT_0) continue;

        CloseHandle(child);
        child = NULL;
        if (fallback_used) break;
        fallback_used = 1;
        child = spawn_fallback(parent);
        if (!child) break;
    }
    if (child) {
        if (WaitForSingleObject(child, 0) == WAIT_TIMEOUT) TerminateProcess(child, 0);
        CloseHandle(child);
    }
    return 0;
}

static int run_configure(void)
{
    char studio[MAX_PATH];
    config_load(&g_cfg);
    if (g_cfg.studio[0]) lstrcpynA(studio, g_cfg.studio, MAX_PATH);
    else install_file("AfterDark.Studio.exe", studio, sizeof(studio));

    if ((INT_PTR)ShellExecuteA(NULL, "open", studio, "--configure", NULL, SW_SHOWNORMAL) <= 32) {
        MessageBoxA(NULL,
                    "After Dark Studio could not be started.\n\n"
                    "Reinstall, or run AfterDark.Studio.exe directly to choose a module.",
                    "After Dark", MB_ICONWARNING | MB_OK);
        return 1;
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    const char *p = cmd;
    (void)inst; (void)prev; (void)show;

    /* The renderer is deliberately isolated. A 1990s module fault must never
     * surface Windows Error Reporting UI over the lock screen or control panel. */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    while (*p == ' ' || *p == '\t') p++;

    /* No arguments at all: Windows means "configure". */
    if (*p == '\0') return run_configure();

    if (*p == '-' || *p == '/') p++;

    switch (*p) {
    case 's': case 'S':
        return run_fullscreen();

    case 'p': case 'P': {
        const char *q = p + 1;
        while (*q == ' ' || *q == ':') q++;
        return run_preview((HWND)(ULONG_PTR)_strtoui64(q, NULL, 0));
    }

    case 'c': case 'C':
        return run_configure();

    case 'a': case 'A':   /* password change on 9x; nothing to do on NT */
    default:
        return 0;
    }
}
