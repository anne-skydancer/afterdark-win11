/* Independent PE32 rewrite of the Classic String Theory module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;

static int string_count(int value)
{
    if (value >= 95) return 220;
    if (value <= 0) return 10;
    return 20 + (value - 12) * 130 / 78;
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int groups = params->iControlValue[0] + 1;
    int strings = string_count(params->iControlValue[1]);
    int color_speed = params->iControlValue[2];
    int clear_frequently = params->iControlValue[3] != 0;
    int minimum;
    int group;
    int draw_index;
    int lines_per_group;

    adm_canvas_fit(params, 960, 540, &width, &height);
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    minimum = width < height ? width : height;
    if (groups < 1) groups = 1;
    if (groups > 4) groups = 4;
    if (color_speed < 1) color_speed = 1;
    if (color_speed > 100) color_speed = 100;
    if (g_canvas.frame == 0 ||
        g_canvas.frame % (clear_frequently ? 45u : 180u) == 0)
        adm_canvas_clear(&g_canvas, 0);
    else if (g_canvas.frame % 8u == 0)
        adm_canvas_fade(&g_canvas, 127, 128);
    lines_per_group = strings < 48 ? strings : 48;

    for (group = 0; group < groups; group++) {
        double group_angle = (double)group * 6.28318530717958647692 /
                             (double)groups;
        int center_x = width / 2 + (int)(cos(group_angle) * minimum * 0.17);
        int center_y = height / 2 + (int)(sin(group_angle) * minimum * 0.17);
        int radius = minimum * (groups == 1 ? 42 : 25) / 100;
        double rotation = (double)g_canvas.frame * 0.008 * (group + 1);
        for (draw_index = 0; draw_index < lines_per_group; draw_index++) {
            int index = (int)(g_canvas.frame * (unsigned)lines_per_group +
                              (unsigned)draw_index) % strings;
            double angle0 = rotation + (double)index * 6.28318530717958647692 /
                                       (double)strings;
            double angle1 = rotation + (double)(index * (group * 2 + 3)) *
                                       6.28318530717958647692 /
                                       (double)strings;
            int x0 = center_x + (int)(cos(angle0) * radius);
            int y0 = center_y + (int)(sin(angle0) * radius);
            int x1 = center_x + (int)(cos(angle1) * radius);
            int y1 = center_y + (int)(sin(angle1) * radius);
            unsigned color = (unsigned)index +
                             g_canvas.frame * (unsigned)color_speed / 10u;
            adm_line(&g_canvas, x0, y0, x1, y1, adm_color(color));
        }
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
        adm_seed(&g_canvas, 0x53545247u ^ GetTickCount());
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