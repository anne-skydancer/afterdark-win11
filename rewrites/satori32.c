/* Independent PE32 rewrite of the Classic Satori module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;
static DWORD g_scene_started;
static int g_random_display;
static int g_random_colors;

static int clarity_size(int value)
{
    if (value >= 80) return 16;
    if (value >= 60) return 8;
    if (value >= 40) return 4;
    if (value >= 20) return 2;
    return 1;
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int display = params->iControlValue[0];
    int colors = params->iControlValue[1];
    int block = clarity_size(params->iControlValue[2]);
    int knots = params->iControlValue[3];
    DWORD now = GetTickCount();
    int knot;

    adm_canvas_fit(params, 960, 540, &width, &height);
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (display < 0 || display > 6) display = 6;
    if (colors < 0 || colors > 13) colors = 13;
    if (g_canvas.frame == 0 || now - g_scene_started >= 6000) {
        g_random_display = adm_random_below(&g_canvas, 6);
        g_random_colors = adm_random_below(&g_canvas, 13);
        g_scene_started = now;
    }
    if (display == 6) display = g_random_display;
    if (colors == 13) colors = g_random_colors;
    if (knots < 1) knots = 1;
    if (knots > 20) knots = 20;
    adm_canvas_fade(&g_canvas, 31, 32);
    for (knot = 0; knot < knots; knot++) {
        double t = g_canvas.frame * 0.018 + knot * 6.28318530717958647692 / knots;
        int center_x = width / 2 + (int)(sin(t * 0.71) * width * 0.34);
        int center_y = height / 2 + (int)(cos(t * 0.83) * height * 0.34);
        int radius = 10 + (knot * 17 + (int)g_canvas.frame) %
                          ((width < height ? width : height) / 5 + 1);
        uint32_t color = adm_color((unsigned)colors * 3u + (unsigned)knot +
                                   g_canvas.frame / 12u);
        if (display == 0 || display == 5)
            adm_filled_ellipse(&g_canvas, center_x, center_y, block, block, color);
        if (display == 1 || display == 5)
            adm_ellipse(&g_canvas, center_x, center_y, radius, radius / 2 + 1, color);
        if (display == 2 || display == 5)
            adm_line(&g_canvas, width / 2, height / 2, center_x, center_y, color);
        if (display == 3 || display == 5) {
            int x;
            for (x = -radius; x <= radius; x += block)
                adm_put_pixel(&g_canvas, center_x + x,
                              center_y + (int)(sin((x + g_canvas.frame) * 0.08) * radius / 3),
                              color);
        }
        if (display == 4 || display == 5)
            adm_filled_triangle(&g_canvas, center_x, center_y - radius / 2,
                                center_x + radius / 3, center_y + radius / 2,
                                center_x - radius / 3, center_y + radius / 2,
                                color, adm_color(14));
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
        adm_canvas_release(&g_canvas); adm_seed(&g_canvas, 0x5341544Fu ^ GetTickCount());
        g_scene_started = 0; return AD_OK;
    case AD_MSG_PREINITIALIZE: return AD_OK;
    case AD_MSG_BLANK:
    case AD_MSG_DRAWFRAME: return render_frame(params) ? AD_OK : 1;
    case AD_MSG_PAINT:
        if (!g_canvas.has_frame) return render_frame(params) ? AD_OK : 1;
        adm_canvas_present(&g_canvas, params); return AD_OK;
    case AD_MSG_CLOSE:
    case AD_MSG_MODULEDESELECTED: adm_canvas_release(&g_canvas); return AD_OK;
    default: return AD_OK;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    if (reason == DLL_PROCESS_DETACH) adm_canvas_release(&g_canvas);
    return TRUE;
}