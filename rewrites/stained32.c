/* Independent PE32 rewrite of the Classic Stained Glass module. */

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

static uint32_t color_amount(uint32_t color, int amount)
{
    unsigned red = (color >> 16) & 0xFFu;
    unsigned green = (color >> 8) & 0xFFu;
    unsigned blue = color & 0xFFu;
    unsigned gray = (red * 30u + green * 59u + blue * 11u) / 100u;
    red = (gray * (unsigned)(100 - amount) + red * (unsigned)amount) / 100u;
    green = (gray * (unsigned)(100 - amount) + green * (unsigned)amount) / 100u;
    blue = (gray * (unsigned)(100 - amount) + blue * (unsigned)amount) / 100u;
    return (red << 16) | (green << 8) | blue;
}

static int render_frame(AD_MODULE32 *params)
{
    int width = params->rcClient.right - params->rcClient.left;
    int height = params->rcClient.bottom - params->rcClient.top;
    int complexity = clamp(params->iControlValue[0], 0, 100);
    int duplication = clamp(params->iControlValue[1], 0, 100);
    int color_percent = clamp(params->iControlValue[2], 0, 100);
    int sectors = 6 + complexity * 18 / 100;
    int patterns = 8 - duplication * 7 / 100;
    int center_x = width / 2;
    int center_y = height / 2;
    int radius = (width < height ? width : height) * 9 / 20;
    double rotation = (double)g_canvas.frame * 0.008;
    int sector;

    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    adm_canvas_clear(&g_canvas, 0);
    for (sector = 0; sector < sectors; sector++) {
        double angle0 = rotation + (double)sector * 6.28318530717958647692 /
                                   (double)sectors;
        double angle1 = rotation + (double)(sector + 1) * 6.28318530717958647692 /
                                   (double)sectors;
        int pattern = sector % patterns;
        double wave0 = 0.84 + 0.14 * sin((double)pattern * 1.7 + rotation * 2.0);
        double wave1 = 0.84 + 0.14 * sin((double)((sector + 1) % patterns) * 1.7 +
                                           rotation * 2.0);
        int inner0_x = center_x + (int)(cos(angle0) * radius * 0.46);
        int inner0_y = center_y + (int)(sin(angle0) * radius * 0.46);
        int inner1_x = center_x + (int)(cos(angle1) * radius * 0.46);
        int inner1_y = center_y + (int)(sin(angle1) * radius * 0.46);
        int outer0_x = center_x + (int)(cos(angle0) * radius * wave0);
        int outer0_y = center_y + (int)(sin(angle0) * radius * wave0);
        int outer1_x = center_x + (int)(cos(angle1) * radius * wave1);
        int outer1_y = center_y + (int)(sin(angle1) * radius * wave1);
        uint32_t inner_color = color_amount(adm_color((unsigned)pattern * 3u +
                                                       g_canvas.frame / 8u),
                                            color_percent);
        uint32_t outer_color = color_amount(adm_color((unsigned)pattern * 3u + 5u +
                                                       g_canvas.frame / 8u),
                                            color_percent);
        adm_filled_triangle(&g_canvas, center_x, center_y,
                            inner0_x, inner0_y, inner1_x, inner1_y,
                            inner_color, 0);
        adm_filled_triangle(&g_canvas, inner0_x, inner0_y,
                            outer0_x, outer0_y, outer1_x, outer1_y,
                            outer_color, 0);
        adm_filled_triangle(&g_canvas, inner0_x, inner0_y,
                            outer1_x, outer1_y, inner1_x, inner1_y,
                            outer_color, 0);
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
        adm_seed(&g_canvas, 0x5354474Cu ^ GetTickCount());
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