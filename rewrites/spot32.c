/* Independent PE32 rewrite of the Classic Spotlight module. */

#include <windows.h>

#include "admkit.h"

typedef struct SPOT {
    double x;
    double y;
    double dx;
    double dy;
    int radius;
    unsigned color;
} SPOT;

static ADM_CANVAS g_canvas;
static SPOT g_spots[4];
static int g_initialized;
static int g_size_value = -1;

static int selected_size(int value)
{
    static const int values[] = {
        0, 5, 10, 16, 22, 28, 34, 40, 46, 52,
        58, 64, 70, 76, 82, 87, 92, 97, 100,
    };
    static const int diameters[] = {
        0, 30, 40, 50, 60, 70, 80, 90, 100, 110,
        120, 130, 140, 150, 160, 170, 180, 190, 200,
    };
    int index;
    for (index = (int)(sizeof(values) / sizeof(values[0])) - 1;
         index > 0; index--)
        if (value >= values[index]) return diameters[index];
    return 0;
}

static void reset_spot(SPOT *spot, int configured_size)
{
    int minimum = g_canvas.width < g_canvas.height ? g_canvas.width
                                                   : g_canvas.height;
    int maximum_radius = (minimum - 1) / 2;
    if (maximum_radius < 1) maximum_radius = 1;
    spot->radius = configured_size > 0 ? configured_size / 2
                                       : 15 + adm_random_below(&g_canvas, 86);
    if (spot->radius > maximum_radius) spot->radius = maximum_radius;
    spot->x = (double)adm_random_below(&g_canvas, g_canvas.width);
    spot->y = (double)adm_random_below(&g_canvas, g_canvas.height);
    spot->dx = (adm_random_below(&g_canvas, 2) ? 1.0 : -1.0) *
               (1.0 + (double)adm_random_below(&g_canvas, 100) / 100.0);
    spot->dy = (adm_random_below(&g_canvas, 2) ? 1.0 : -1.0) *
               (1.0 + (double)adm_random_below(&g_canvas, 100) / 100.0);
    spot->color = (unsigned)adm_random_below(&g_canvas, 16);
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int configured_size = selected_size(params->iControlValue[0]);
    int speed = params->iControlValue[1];
    int count = params->iControlValue[2];
    int index;
    int resized;

    adm_canvas_fit(params, 1280, 720, &width, &height);
    resized = g_canvas.width != width || g_canvas.height != height;
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (speed < 1) speed = 1;
    if (speed > 20) speed = 20;
    if (count < 1) count = 1;
    if (count > 4) count = 4;
    if (width < 3 || height < 3) {
        adm_canvas_clear(&g_canvas, 0);
        adm_put_pixel(&g_canvas, width / 2, height / 2, adm_color(14));
        g_canvas.frame++;
        g_canvas.has_frame = 1;
        adm_canvas_present(&g_canvas, params);
        return 1;
    }
    if (resized || g_size_value != params->iControlValue[0])
        g_initialized = 0;
    if (!g_initialized) {
        for (index = 0; index < 4; index++)
            reset_spot(&g_spots[index], configured_size);
        g_initialized = 1;
        g_size_value = params->iControlValue[0];
    }
    adm_canvas_clear(&g_canvas, 0x00040508u);
    for (index = 0; index < count; index++) {
        SPOT *spot = &g_spots[index];
        double factor = 0.35 + (double)speed / 10.0;
        spot->x += spot->dx * factor;
        spot->y += spot->dy * factor;
        if (spot->x - spot->radius < 0) {
            spot->x = spot->radius;
            spot->dx = -spot->dx;
        }
        if (spot->x + spot->radius >= width) {
            spot->x = width - spot->radius - 1;
            spot->dx = -spot->dx;
        }
        if (spot->y - spot->radius < 0) {
            spot->y = spot->radius;
            spot->dy = -spot->dy;
        }
        if (spot->y + spot->radius >= height) {
            spot->y = height - spot->radius - 1;
            spot->dy = -spot->dy;
        }
        adm_filled_ellipse(&g_canvas, (int)spot->x, (int)spot->y,
                           spot->radius, spot->radius,
                           adm_color(spot->color + g_canvas.frame / 15u));
        adm_ellipse(&g_canvas, (int)spot->x, (int)spot->y,
                    spot->radius, spot->radius, adm_color(14));
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
        adm_seed(&g_canvas, 0x53504F54u ^ GetTickCount());
        g_initialized = 0;
        g_size_value = -1;
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
        g_initialized = 0;
        g_size_value = -1;
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