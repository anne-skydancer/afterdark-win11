/* Independent PE32 rewrite of the Classic Spheres module. */

#include <windows.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;

static int clamp(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int render_frame(AD_MODULE32 *params)
{
    int width = params->rcClient.right - params->rcClient.left;
    int height = params->rcClient.bottom - params->rcClient.top;
    int maximum_percent = clamp(params->iControlValue[0], 10, 100);
    int offset = clamp(params->iControlValue[1], 0, 10);
    int clear_every = clamp(params->iControlValue[2], 1, 200);
    int frequent = params->iControlValue[3] != 0;
    int minimum_dimension = width < height ? width : height;
    int maximum_radius = minimum_dimension * maximum_percent / 220;
    int radius;
    int center_x;
    int center_y;
    int drift_x;
    int drift_y;
    unsigned clear_interval = frequent ? (unsigned)(clear_every / 2)
                                       : (unsigned)clear_every;
    uint32_t color;

    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (clear_interval < 1) clear_interval = 1;
    if (g_canvas.frame % clear_interval == 0) adm_canvas_clear(&g_canvas, 0);
    if (maximum_radius < 4) maximum_radius = 4;
    radius = 4 + adm_random_below(&g_canvas, maximum_radius - 3);
    drift_x = offset * width / 40;
    drift_y = offset * height / 40;
    center_x = width / 2 + adm_random_below(&g_canvas, drift_x * 2 + 1) - drift_x;
    center_y = height / 2 + adm_random_below(&g_canvas, drift_y * 2 + 1) - drift_y;
    color = adm_color(g_canvas.frame + (unsigned)adm_random_below(&g_canvas, 16));
    adm_ellipse(&g_canvas, center_x, center_y, radius, radius, color);
    if (radius > 8)
        adm_ellipse(&g_canvas, center_x - radius / 5, center_y - radius / 5,
                    radius / 4, radius / 4, adm_color(14));

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
        adm_seed(&g_canvas, 0x53504852u ^ GetTickCount());
        return AD_OK;
    case AD_MSG_PREINITIALIZE:
        return AD_OK;
    case AD_MSG_BLANK:
        if (!adm_canvas_resize(&g_canvas,
                               params->rcClient.right - params->rcClient.left,
                               params->rcClient.bottom - params->rcClient.top))
            return 1;
        adm_canvas_clear(&g_canvas, 0);
        g_canvas.frame = 0;
        return render_frame(params) ? AD_OK : 1;
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