/* Independent PE32 rewrite of the Classic Spiral Gyra module. */

#include <windows.h>
#include <math.h>

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
    int maximum = clamp(params->iControlValue[0], 20, 360);
    int minimum_percent = clamp(params->iControlValue[1], 0, 100);
    int color_speed = clamp(params->iControlValue[2], 0, 100);
    int minimum = maximum * minimum_percent / 100;
    int radius;
    int center_x;
    int center_y;
    int lines;
    int index;
    double pulse;
    double rotation;

    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    adm_canvas_clear(&g_canvas, 0);
    if (minimum < 2) minimum = 2;
    pulse = (sin((double)g_canvas.frame * 0.035) + 1.0) * 0.5;
    lines = minimum + (int)((double)(maximum - minimum) * pulse);
    radius = (width < height ? width : height) * 9 / 20;
    center_x = width / 2;
    center_y = height / 2;
    rotation = (double)g_canvas.frame * 0.012;

    for (index = 0; index < lines; index++) {
        double angle = rotation + (double)index * 6.28318530717958647692 /
                                  (double)lines;
        double twist = rotation * 1.7 + (double)index * 2.39996322972865332;
        int outer_x = center_x + (int)(cos(angle) * radius);
        int outer_y = center_y + (int)(sin(angle) * radius);
        int inner_radius = radius * (20 + (index * 80 / lines)) / 100;
        int inner_x = center_x + (int)(cos(twist) * inner_radius);
        int inner_y = center_y + (int)(sin(twist) * inner_radius);
        unsigned color_index = (unsigned)index;
        if (color_speed > 0)
            color_index += g_canvas.frame * (unsigned)color_speed / 12u;
        adm_line(&g_canvas, outer_x, outer_y, inner_x, inner_y,
                 adm_color(color_index));
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
        adm_seed(&g_canvas, 0x53504952u ^ GetTickCount());
        return AD_OK;
    case AD_MSG_PREINITIALIZE:
        return AD_OK;
    case AD_MSG_BLANK:
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