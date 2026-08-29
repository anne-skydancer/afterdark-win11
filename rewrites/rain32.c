/* Independent PE32 rewrite of the Classic Hard Rain module. */

#include <windows.h>

#include "admkit.h"

typedef struct DROP {
    int x;
    int y;
    int speed;
    unsigned color;
} DROP;

static ADM_CANVAS g_canvas;
static DROP g_drops[216];
static int g_drop_count;

static void reset_drop(DROP *drop, int at_top)
{
    drop->x = adm_random_below(&g_canvas, g_canvas.width + 160) - 80;
    drop->y = at_top ? -adm_random_below(&g_canvas, g_canvas.height + 1)
                     : adm_random_below(&g_canvas, g_canvas.height + 1);
    drop->speed = 8 + adm_random_below(&g_canvas, 13);
    drop->color = (unsigned)adm_random_below(&g_canvas, 6) + 10u;
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int density = params->iControlValue[0];
    int size = params->iControlValue[1];
    int clear_frequently = params->iControlValue[3] != 0;
    int count;
    int index;
    int resized;

    adm_canvas_fit(params, 1280, 720, &width, &height);
    resized = g_canvas.width != width || g_canvas.height != height;
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (resized) g_drop_count = 0;
    if (density < 1) density = 1;
    if (density > 9) density = 9;
    if (size < 5) size = 5;
    if (size > 35) size = 35;
    count = density * 24;
    while (g_drop_count < count)
        reset_drop(&g_drops[g_drop_count++], 0);
    g_drop_count = count;
    if (clear_frequently || g_canvas.frame == 0)
        adm_canvas_clear(&g_canvas, 0);
    else if (g_canvas.frame % 4u == 0)
        adm_canvas_fade(&g_canvas, 31, 32);

    for (index = 0; index < count; index++) {
        DROP *drop = &g_drops[index];
        int length = size + drop->speed;
        int next_x = drop->x - length / 4;
        int next_y = drop->y + length;
        adm_line(&g_canvas, drop->x, drop->y, next_x, next_y,
                 adm_color(drop->color));
        drop->x -= drop->speed / 4;
        drop->y += drop->speed;
        if (drop->y - length > height || drop->x < -length)
            reset_drop(drop, 1);
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
        adm_seed(&g_canvas, 0x5241494Eu ^ GetTickCount());
        g_drop_count = 0;
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
        g_drop_count = 0;
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