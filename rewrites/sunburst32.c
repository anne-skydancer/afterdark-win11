/* Independent PE32 rewrite of the Classic Sunburst module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;
static DWORD g_started;
static DWORD g_last_fade;
static unsigned g_last_phase = ~0u;

static DWORD cadence_milliseconds(int value)
{
    if (value >= 90) return 12;
    if (value >= 70) return 22;
    if (value >= 50) return 35;
    if (value >= 30) return 55;
    return 80;
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    DWORD step_ms = cadence_milliseconds(params->iControlValue[0]);
    DWORD now = GetTickCount();
    DWORD fade_steps;
    unsigned phase;
    unsigned first_phase;
    unsigned draw_phase;

    adm_canvas_fit(params, 1280, 720, &width, &height);
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (g_started == 0) g_started = now;
    if (g_last_fade == 0) g_last_fade = now;
    phase = (now - g_started) / step_ms;
    fade_steps = (now - g_last_fade) / 16u;
    if (fade_steps > 64u) {
        adm_canvas_clear(&g_canvas, 0);
        g_last_fade = now;
    } else {
        DWORD fade;
        for (fade = 0; fade < fade_steps; fade++)
            adm_canvas_fade(&g_canvas, 15, 16);
        g_last_fade += fade_steps * 16u;
    }
    if (g_last_phase == ~0u || phase < g_last_phase)
        first_phase = phase;
    else
        first_phase = g_last_phase + 1u;
    if (first_phase <= phase && phase - first_phase > 64u)
        first_phase = phase - 64u;
    for (draw_phase = first_phase; draw_phase <= phase; draw_phase++) {
        int center_x = width / 2 +
                       (int)(sin(draw_phase * 0.013) * width * 0.16);
        int center_y = height / 2 +
                       (int)(cos(draw_phase * 0.017) * height * 0.13);
        int maximum = (width < height ? width : height) * 47 / 100;
        int ray;
        for (ray = 0; ray < 72; ray++) {
            double angle = (double)ray * 6.28318530717958647692 / 72.0 +
                           draw_phase * 0.006;
            int reach = maximum * (65 + (ray * 37 + (int)draw_phase) % 36) /
                        100;
            int x = center_x + (int)(cos(angle) * reach);
            int y = center_y + (int)(sin(angle) * reach);
            adm_line(&g_canvas, center_x, center_y, x, y,
                     adm_color((unsigned)ray / 4u + draw_phase / 3u));
        }
        adm_filled_ellipse(&g_canvas, center_x, center_y, 10, 10,
                           adm_color(14));
    }
    g_last_phase = phase;
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
        g_started = 0;
        g_last_fade = 0;
        g_last_phase = ~0u;
        adm_seed(&g_canvas, 0x53554E42u ^ GetTickCount()); return AD_OK;
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