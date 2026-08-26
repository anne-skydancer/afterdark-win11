/* Independent PE32 rewrite of the Classic Strange Attractor module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;
static double g_x;
static double g_y;
static double g_a;
static double g_b;
static double g_c;
static double g_d;
static DWORD g_scene_started;

static DWORD duration_milliseconds(int value)
{
    if (value >= 100) return 3600000;
    if (value >= 94) return 2700000;
    if (value >= 87) return 1800000;
    if (value >= 80) return 1200000;
    if (value >= 74) return 900000;
    if (value >= 67) return 600000;
    if (value >= 60) return 300000;
    if (value >= 54) return 120000;
    if (value >= 47) return 60000;
    if (value >= 40) return 45000;
    if (value >= 34) return 30000;
    if (value >= 27) return 20000;
    if (value >= 20) return 15000;
    if (value >= 15) return 10000;
    return 5000;
}

static void reset_attractor(void)
{
    static const double presets[][4] = {
        { -1.40,  1.60,  1.00,  0.70 },
        { -1.70,  1.80, -1.90, -0.40 },
        { -1.80, -2.00, -0.50, -0.90 },
        {  1.70,  1.70,  0.60,  1.20 },
        { -1.40,  1.50, -1.80, -1.90 },
    };
    unsigned preset = (unsigned)adm_random_below(
        &g_canvas, (int)(sizeof(presets) / sizeof(presets[0])));
    g_x = 0.1;
    g_y = 0.1;
    g_a = presets[preset][0];
    g_b = presets[preset][1];
    g_c = presets[preset][2];
    g_d = presets[preset][3];
    adm_canvas_clear(&g_canvas, 0);
    g_scene_started = GetTickCount();
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    DWORD duration = duration_milliseconds(params->iControlValue[0]);
    DWORD now = GetTickCount();
    int color_speed = params->iControlValue[1];
    int point;

    adm_canvas_fit(params, 1280, 720, &width, &height);
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (color_speed < 0) color_speed = 0;
    if (color_speed > 100) color_speed = 100;
    if (g_canvas.frame == 0 || now - g_scene_started >= duration)
        reset_attractor();
    adm_canvas_fade(&g_canvas, 255, 256);
    for (point = 0; point < 1600; point++) {
        double next_x = sin(g_a * g_y) + g_c * cos(g_a * g_x);
        double next_y = sin(g_b * g_x) + g_d * cos(g_b * g_y);
        int pixel_x;
        int pixel_y;
        unsigned color;
        g_x = next_x;
        g_y = next_y;
        pixel_x = width / 2 + (int)(g_x * width * 0.19);
        pixel_y = height / 2 + (int)(g_y * height * 0.19);
        color = color_speed == 0 ? 14u :
                (unsigned)point / 80u + g_canvas.frame * (unsigned)color_speed / 8u;
        adm_put_pixel(&g_canvas, pixel_x, pixel_y, adm_color(color));
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
        adm_seed(&g_canvas, 0x53545241u ^ GetTickCount());
        g_scene_started = 0;
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