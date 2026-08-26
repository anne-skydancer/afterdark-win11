/* Independent PE32 rewrite of the Classic Photon module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;
static DWORD g_last_burst;

static int particle_length(int value)
{
    if (value >= 100) return 32;
    if (value >= 94) return 28;
    if (value >= 89) return 24;
    if (value >= 84) return 20;
    if (value >= 79) return 16;
    if (value >= 74) return 14;
    if (value >= 69) return 12;
    if (value >= 64) return 10;
    if (value >= 59) return 9;
    if (value >= 54) return 8;
    if (value >= 49) return 7;
    if (value >= 44) return 6;
    if (value >= 39) return 5;
    if (value >= 33) return 4;
    if (value >= 26) return 3;
    if (value >= 18) return 2;
    return 1;
}

static DWORD delay_milliseconds(int value)
{
    if (value >= 90) return 30000;
    if (value >= 80) return 10000;
    if (value >= 60) return 5000;
    if (value >= 40) return 2000;
    if (value >= 20) return 1000;
    if (value >= 10) return 500;
    return 0;
}

static void draw_burst(int center_x, int center_y, int length, int kind)
{
    int rays = kind == 4 ? 36 : 18 + kind * 6;
    int ray;
    int maximum = (g_canvas.width < g_canvas.height ? g_canvas.width
                                                    : g_canvas.height) *
                  (18 + length) / 70;
    for (ray = 0; ray < rays; ray++) {
        double angle = (double)ray * 6.28318530717958647692 / (double)rays +
                       (double)adm_random_below(&g_canvas, 100) / 700.0;
        int reach = maximum * (70 + adm_random_below(&g_canvas, 31)) / 100;
        int end_x = center_x + (int)(cos(angle) * reach);
        int end_y = center_y + (int)(sin(angle) * reach);
        uint32_t color = kind == 1 ? adm_color(14) :
                         kind == 2 ? adm_color(2) :
                         kind == 3 ? adm_color(3) :
                         kind == 4 ? adm_color(12) :
                                     adm_color((unsigned)ray + g_canvas.frame);
        adm_line(&g_canvas, center_x, center_y, end_x, end_y, color);
        if (length > 10)
            adm_ellipse(&g_canvas, end_x, end_y, length / 6, length / 6, color);
    }
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int length = particle_length(params->iControlValue[0]);
    DWORD delay = delay_milliseconds(params->iControlValue[1]);
    DWORD now = GetTickCount();
    int centered = params->iControlValue[2] != 0;
    int kind = params->iControlValue[3];

    adm_canvas_fit(params, 1280, 720, &width, &height);
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (kind < 0 || kind > 4) kind = 0;
    if (delay < 5000 && g_canvas.frame % 8u == 0)
        adm_canvas_fade(&g_canvas, 31, 32);
    if (g_canvas.frame == 0 || delay == 0 || now - g_last_burst >= delay) {
        int center_x = centered ? width / 2 : adm_random_below(&g_canvas, width);
        int center_y = centered ? height / 2 : adm_random_below(&g_canvas, height);
        draw_burst(center_x, center_y, length, kind);
        g_last_burst = now;
    }
    g_canvas.frame++;
    g_canvas.has_frame = 1;
    adm_canvas_present(&g_canvas, params);
    return 1;
}

__declspec(dllexport) int AD_STDCALL Module(AD_MODULE32 *params)
{
    if (!params || params->cbSize < AD_MODULE32_SIZE) return 1;
    switch (params->dwMessage) {
    case AD_MSG_MODULESELECTED:
        adm_canvas_release(&g_canvas);
        adm_seed(&g_canvas, 0x50484F54u ^ GetTickCount());
        g_last_burst = 0;
        return AD_OK;
    case AD_MSG_PREINITIALIZE:
        return AD_OK;
    case AD_MSG_BLANK:
    {
        int width;
        int height;
        adm_canvas_fit(params, 1280, 720, &width, &height);
        if (!adm_canvas_resize(&g_canvas, width, height))
            return 1;
        adm_canvas_clear(&g_canvas, 0);
        g_canvas.frame = 0;
        g_last_burst = 0;
        return render_frame(params) ? AD_OK : 1;
    }
    case AD_MSG_DRAWFRAME:
        return render_frame(params) ? AD_OK : 1;
    case AD_MSG_PAINT:
        if (!g_canvas.has_frame) return render_frame(params) ? AD_OK : 1;
        adm_canvas_present(&g_canvas, params);
        return AD_OK;
    case AD_MSG_CLOSE:
    case AD_MSG_MODULEDESELECTED:
        adm_canvas_release(&g_canvas);
        return AD_OK;
    default:
        return AD_OK;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    if (reason == DLL_PROCESS_DETACH) adm_canvas_release(&g_canvas);
    return TRUE;
}