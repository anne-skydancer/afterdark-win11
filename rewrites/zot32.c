/* Independent PE32 rewrite of the Classic Zot! module. */

#include <windows.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;

static void draw_branch(int start_x, int start_y, int direction, int length,
                        uint32_t color)
{
    int x = start_x;
    int y = start_y;
    int segment;
    for (segment = 0; segment < 5; segment++) {
        int next_x = x + direction * (length / 5) +
                     adm_random_below(&g_canvas, 15) - 7;
        int next_y = y + length / 8 + adm_random_below(&g_canvas, 9);
        adm_line(&g_canvas, x, y, next_x, next_y, color);
        x = next_x;
        y = next_y;
    }
}

static void draw_bolt(int forkiness)
{
    int x = g_canvas.width / 5 + adm_random_below(&g_canvas,
                                                   g_canvas.width * 3 / 5);
    int y = 0;
    int target_x = adm_random_below(&g_canvas, g_canvas.width);
    int segments = 18;
    int segment;
    uint32_t color = adm_color(14);

    for (segment = 1; segment <= segments; segment++) {
        int remaining = segments - segment + 1;
        int next_y = g_canvas.height * segment / segments;
        int next_x = x + (target_x - x) / remaining +
                     adm_random_below(&g_canvas, g_canvas.width / 12 + 1) -
                     g_canvas.width / 24;
        adm_line(&g_canvas, x, y, next_x, next_y, color);
        adm_line(&g_canvas, x + 1, y, next_x + 1, next_y, adm_color(6));
        if (adm_random_below(&g_canvas, 100) < forkiness) {
            int direction = adm_random_below(&g_canvas, 2) ? 1 : -1;
            draw_branch(next_x, next_y, direction,
                        g_canvas.width / 5 + adm_random_below(&g_canvas,
                                                              g_canvas.width / 5 + 1),
                        adm_color(15));
        }
        x = next_x;
        y = next_y;
    }
}

static unsigned bolt_interval(int value)
{
    if (value >= 75) return 8;
    if (value >= 50) return 24;
    if (value >= 25) return 55;
    return 110;
}

static int render_frame(AD_MODULE32 *params)
{
    int width = params->rcClient.right - params->rcClient.left;
    int height = params->rcClient.bottom - params->rcClient.top;
    int forkiness = params->iControlValue[0];
    unsigned interval = bolt_interval(params->iControlValue[2]);

    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (forkiness < 0) forkiness = 0;
    if (forkiness > 100) forkiness = 100;
    adm_canvas_fade(&g_canvas, 63, 64);
    if (g_canvas.frame == 0 || g_canvas.frame % interval == 0)
        draw_bolt(forkiness);
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
        adm_seed(&g_canvas, 0x5A4F5421u ^ GetTickCount());
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