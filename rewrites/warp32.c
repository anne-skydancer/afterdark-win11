/* Independent PE32 rewrite of the Classic Warp! module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

typedef struct STAR {
    double x;
    double y;
    double z;
    unsigned color;
    int large;
} STAR;

static ADM_CANVAS g_canvas;
static STAR g_stars[200];
static int g_star_count;

static void reset_star(STAR *star, int outward, int distributed)
{
    star->x = ((double)adm_random_below(&g_canvas, 2001) - 1000.0) / 1000.0;
    star->y = ((double)adm_random_below(&g_canvas, 2001) - 1000.0) / 1000.0;
    star->z = distributed
        ? 0.12 + (double)adm_random_below(&g_canvas, 981) / 1000.0
        : (outward ? 1.1 : 0.12);
    star->color = (unsigned)adm_random_below(&g_canvas, 16);
    star->large = adm_random_below(&g_canvas, 2);
}

static double speed_for(int value, int *outward)
{
    *outward = value <= 38;
    if (value <= 0 || value >= 88) return 0.075;
    if (value <= 12 || value >= 75) return 0.050;
    if (value <= 25 || value >= 55) return 0.025;
    return 0.095;
}

static int render_frame(AD_MODULE32 *params)
{
    int width = params->rcClient.right - params->rcClient.left;
    int height = params->rcClient.bottom - params->rcClient.top;
    int count = params->iControlValue[1];
    int size_mode = params->iControlValue[2];
    int colored = params->iControlValue[3] != 0;
    int outward;
    double speed = speed_for(params->iControlValue[0], &outward);
    double scale;
    int index;

    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (count < 1) count = 1;
    if (count > 200) count = 200;
    if (size_mode < 0 || size_mode > 2) size_mode = 0;
    while (g_star_count < count)
        reset_star(&g_stars[g_star_count++], outward, 1);
    g_star_count = count;
    scale = (double)(width < height ? width : height) * 0.42;
    adm_canvas_clear(&g_canvas, 0);

    for (index = 0; index < count; index++) {
        STAR *star = &g_stars[index];
        double previous_z = star->z;
        int previous_x = width / 2 + (int)(star->x * scale / previous_z);
        int previous_y = height / 2 + (int)(star->y * scale / previous_z);
        int x;
        int y;
        int large;
        int wrapped = 0;
        uint32_t color;
        star->z += outward ? -speed : speed;
        if (star->z < 0.08 || star->z > 1.2) {
            reset_star(star, outward, 0);
            wrapped = 1;
        }
        x = width / 2 + (int)(star->x * scale / star->z);
        y = height / 2 + (int)(star->y * scale / star->z);
        large = size_mode == 1 || (size_mode == 2 && star->large);
        color = colored ? adm_color(star->color) : adm_color(14);
        if (!wrapped) adm_line(&g_canvas, previous_x, previous_y, x, y, color);
        adm_put_pixel(&g_canvas, x, y, color);
        if (large) {
            adm_put_pixel(&g_canvas, x - 1, y, color);
            adm_put_pixel(&g_canvas, x + 1, y, color);
            adm_put_pixel(&g_canvas, x, y - 1, color);
            adm_put_pixel(&g_canvas, x, y + 1, color);
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
        adm_seed(&g_canvas, 0x57415250u ^ GetTickCount());
        g_star_count = 0;
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
        g_star_count = 0;
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