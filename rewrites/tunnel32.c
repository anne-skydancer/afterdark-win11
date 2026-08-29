/* Independent PE32 rewrite of the Classic Tunnel module. */

#include <windows.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;

static int render_frame(AD_MODULE32 *params)
{
    const int rings = 22;
    const int spacing = 18;
    int width = params->rcClient.right - params->rcClient.left;
    int height = params->rcClient.bottom - params->rcClient.top;
    int direction = params->iControlValue[0] == 1 ? 1 : 0;
    int shape = params->iControlValue[1];
    int center_x = width / 2;
    int center_y = height / 2;
    int maximum_x = width * 3 / 5;
    int maximum_y = height * 3 / 5;
    int phase = (int)(g_canvas.frame % spacing);
    int ring;

    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (shape < 0 || shape > 2) shape = 1;
    adm_canvas_clear(&g_canvas, 0);

    for (ring = 0; ring < rings; ring++) {
        int distance = ring * spacing + (direction ? phase : spacing - phase);
        int half_width = maximum_x * distance / (rings * spacing);
        int half_height = maximum_y * distance / (rings * spacing);
        int selected = shape == 2 ? adm_random_below(&g_canvas, 2) : shape;
        uint32_t color = adm_color((unsigned)ring + g_canvas.frame / 3u);
        if (half_width < 2 || half_height < 2) continue;
        if (selected == 1) {
            int radius = (half_width < half_height ? half_width : half_height) / 5;
            adm_round_rectangle(&g_canvas,
                                center_x - half_width, center_y - half_height,
                                center_x + half_width, center_y + half_height,
                                radius, color);
        } else {
            adm_rectangle(&g_canvas,
                          center_x - half_width, center_y - half_height,
                          center_x + half_width, center_y + half_height, color);
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
        adm_seed(&g_canvas, 0x54554E4Eu ^ GetTickCount());
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