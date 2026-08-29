/* Independent PE32 rewrite of the Classic Nirvana module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

typedef struct FLOWER {
    double x;
    double y;
    double angle;
    unsigned color;
} FLOWER;

static ADM_CANVAS g_canvas;
static FLOWER g_flow[96];
static int g_initialized;
static DWORD g_started;
static DWORD g_color_changed;
static unsigned g_color_phase;
static int g_random_style;

static DWORD redraw_ms(int value)
{
    if (value >= 80) return 3600000;
    if (value >= 60) return 1800000;
    if (value >= 40) return 600000;
    if (value >= 20) return 300000;
    return 180000;
}

static DWORD color_ms(int value)
{
    if (value >= 75) return 1000;
    if (value >= 50) return 5000;
    if (value >= 25) return 15000;
    return 0;
}

static void reset_flow(void)
{
    int index;
    for (index = 0; index < 96; index++) {
        g_flow[index].x = (double)adm_random_below(&g_canvas, g_canvas.width);
        g_flow[index].y = (double)adm_random_below(&g_canvas, g_canvas.height);
        g_flow[index].angle = (double)adm_random_below(&g_canvas, 6284) / 1000.0;
        g_flow[index].color = (unsigned)adm_random_below(&g_canvas, 16);
    }
    adm_canvas_clear(&g_canvas, 0);
    g_started = GetTickCount();
    g_initialized = 1;
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int style = params->iControlValue[0];
    DWORD redraw = redraw_ms(params->iControlValue[1]);
    int activity = params->iControlValue[2];
    DWORD change = color_ms(params->iControlValue[3]);
    DWORD now = GetTickCount();
    int count;
    int index;
    int resized;
    double speed;
    adm_canvas_fit(params, 1280, 720, &width, &height);
    resized = g_canvas.width != width || g_canvas.height != height;
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (style < 0 || style > 7) style = 7;
    if (resized) g_initialized = 0;
    if (style == 7) style = g_random_style;
    if (activity < 0) activity = 0;
    if (activity > 100) activity = 100;
    if (!g_initialized || now - g_started >= redraw) reset_flow();
    if (change > 0 && now - g_color_changed >= change) {
        g_color_phase++;
        g_random_style = adm_random_below(&g_canvas, 7);
        g_color_changed = now;
    }
    count = 20 + activity * 76 / 100;
    speed = 0.45 + activity * 0.025;
    adm_canvas_fade(&g_canvas, 63, 64);
    for (index = 0; index < count; index++) {
        FLOWER *flow = &g_flow[index];
        int old_x = (int)flow->x;
        int old_y = (int)flow->y;
        int wrapped = 0;
        double field = sin(flow->x * 0.013 + g_color_phase * 0.11) +
                       cos(flow->y * 0.017 - g_color_phase * 0.07);
        uint32_t color;
        flow->angle += field * 0.035;
        flow->x += cos(flow->angle) * speed;
        flow->y += sin(flow->angle) * speed;
        if (flow->x < 0 || flow->x >= width || flow->y < 0 || flow->y >= height) {
            flow->x = (double)adm_random_below(&g_canvas, width);
            flow->y = (double)adm_random_below(&g_canvas, height);
            wrapped = 1;
        }
        color = style == 1 ? adm_color((unsigned)(index % 2) * 8u + g_color_phase) :
                style == 2 ? adm_color((unsigned)(index % 3) * 5u) :
                style == 4 ? adm_color(5u + (unsigned)(index % 3)) :
                style == 5 ? adm_color(14) :
                adm_color(flow->color + g_color_phase + (unsigned)style * 2u);
        if (!wrapped)
            adm_line(&g_canvas, old_x, old_y, (int)flow->x, (int)flow->y, color);
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
        adm_canvas_release(&g_canvas); adm_seed(&g_canvas, 0x4E495256u ^ GetTickCount());
        g_initialized = 0; g_started = 0; g_color_changed = GetTickCount();
        g_color_phase = 0; g_random_style = adm_random_below(&g_canvas, 7);
        return AD_OK;
    case AD_MSG_PREINITIALIZE: return AD_OK;
    case AD_MSG_BLANK:
    case AD_MSG_DRAWFRAME: return render_frame(params) ? AD_OK : 1;
    case AD_MSG_PAINT:
        if (!g_canvas.has_frame) return render_frame(params) ? AD_OK : 1;
        adm_canvas_present(&g_canvas, params); return AD_OK;
    case AD_MSG_CLOSE:
    case AD_MSG_MODULEDESELECTED:
        adm_canvas_release(&g_canvas); g_initialized = 0; return AD_OK;
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