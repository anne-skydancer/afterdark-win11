/* Independent PE32 rewrite of the Classic Mountains module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;
static DWORD g_scene_started;
static int g_random_view;
static int g_random_planet;

static double height_at(double x, double y, unsigned seed, int view)
{
    double value = sin(x * 1.7 + seed * 0.17) * 0.45 +
                   cos(y * 2.1 - seed * 0.11) * 0.35 +
                   sin((x + y) * 3.7 + seed * 0.07) * 0.20;
    if (view == 4) value = floor((value + 1.0) * 4.0) / 4.0 - 1.0;
    return value;
}

static uint32_t planet_color(int planet, double height, unsigned shade)
{
    static const unsigned bases[][2] = {
        { 5, 8 }, { 1, 9 }, { 2, 3 }, { 8, 4 }, { 1, 6 },
        { 9, 1 }, { 7, 3 }, { 3, 7 }, { 5, 13 },
    };
    unsigned index = height > 0.15 ? bases[planet][1] : bases[planet][0];
    return adm_color(index + shade % 2u);
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int view = params->iControlValue[0];
    int planet = params->iControlValue[1];
    int complexity = params->iControlValue[2];
    int zoom = params->iControlValue[3];
    int grid;
    int row;
    int column;
    double scale;
    DWORD now = GetTickCount();
    unsigned seed;

    adm_canvas_fit(params, 1280, 720, &width, &height);
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (view < 0 || view > 5) view = 5;
    if (planet < 0 || planet > 9) planet = 9;
    if (g_canvas.frame == 0 || now - g_scene_started >= 6000) {
        g_random_view = adm_random_below(&g_canvas, 5);
        g_random_planet = adm_random_below(&g_canvas, 9);
        g_scene_started = now;
    }
    seed = g_canvas.random;
    if (view == 5) view = g_random_view;
    if (planet == 9) planet = g_random_planet;
    if (complexity < 3) complexity = 3;
    if (complexity > 6) complexity = 6;
    if (zoom < 0) zoom = 0;
    if (zoom > 100) zoom = 100;
    grid = 1 << complexity;
    scale = 0.75 + (double)zoom / 180.0;
    adm_canvas_clear(&g_canvas, 0x00030508u);

    for (row = 0; row < grid; row++) {
        for (column = 0; column < grid; column++) {
            double x0 = ((double)column / grid - 0.5) * 4.0 / scale;
            double y0 = ((double)row / grid - 0.5) * 3.0 / scale;
            double x1 = ((double)(column + 1) / grid - 0.5) * 4.0 / scale;
            double y1 = ((double)(row + 1) / grid - 0.5) * 3.0 / scale;
            double h00 = height_at(x0, y0, seed, view);
            double h10 = height_at(x1, y0, seed, view);
            double h01 = height_at(x0, y1, seed, view);
            double h11 = height_at(x1, y1, seed, view);
            int sx00 = width / 2 + (column - row) * width / (grid * 2);
            int sy00 = height / 5 + (column + row) * height / (grid * 3) -
                       (int)(h00 * height / 5);
            int sx10 = width / 2 + (column + 1 - row) * width / (grid * 2);
            int sy10 = height / 5 + (column + 1 + row) * height / (grid * 3) -
                       (int)(h10 * height / 5);
            int sx01 = width / 2 + (column - row - 1) * width / (grid * 2);
            int sy01 = height / 5 + (column + row + 1) * height / (grid * 3) -
                       (int)(h01 * height / 5);
            int sx11 = width / 2 + (column - row) * width / (grid * 2);
            int sy11 = height / 5 + (column + row + 2) * height / (grid * 3) -
                       (int)(h11 * height / 5);
            uint32_t color0 = planet_color(planet, h00,
                                            (unsigned)(row + column));
            uint32_t color1 = planet_color(planet, h11,
                                            (unsigned)(row + column + 1));
            uint32_t outline = 0;
            if (view == 0) {
                if (row == 0 || column == 0 || row == grid - 1 ||
                    column == grid - 1) {
                    adm_line(&g_canvas, sx00, sy00, sx10, sy10, color0);
                    adm_line(&g_canvas, sx00, sy00, sx01, sy01, color0);
                }
                continue;
            }
            if (view == 1) {
                adm_line(&g_canvas, sx00, sy00, sx10, sy10, color0);
                adm_line(&g_canvas, sx00, sy00, sx01, sy01, color0);
                continue;
            }
            if (view == 3) {
                color0 = adm_color((unsigned)(row * 3 + column * 5));
                color1 = adm_color((unsigned)(row * 7 + column * 2 + 4));
                outline = adm_color(14);
            }
            adm_filled_triangle(&g_canvas, sx00, sy00, sx10, sy10, sx01, sy01,
                                color0, outline);
            adm_filled_triangle(&g_canvas, sx10, sy10, sx11, sy11, sx01, sy01,
                                color1, outline);
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
        g_scene_started = 0;
        adm_seed(&g_canvas, 0x4D4F554Eu ^ GetTickCount());
        return AD_OK;
    case AD_MSG_PREINITIALIZE: return AD_OK;
    case AD_MSG_BLANK:
    case AD_MSG_DRAWFRAME: return render_frame(params) ? AD_OK : 1;
    case AD_MSG_PAINT:
        if (!g_canvas.has_frame) return render_frame(params) ? AD_OK : 1;
        adm_canvas_present(&g_canvas, params); return AD_OK;
    case AD_MSG_CLOSE:
    case AD_MSG_MODULEDESELECTED:
        adm_canvas_release(&g_canvas); return AD_OK;
    default: return AD_OK;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    if (reason == DLL_PROCESS_DETACH) adm_canvas_release(&g_canvas);
    return TRUE;
}