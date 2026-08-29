/* Independent PE32 rewrite of the Classic Vertigo module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

static ADM_CANVAS g_canvas;
static DWORD g_scene_started;
static DWORD g_animation_started;
static unsigned g_scene;
static unsigned g_random_stride;
static unsigned g_random_offset;

static DWORD delay_ms(int value)
{
    if (value >= 90) return 60000;
    if (value >= 80) return 50000;
    if (value >= 70) return 40000;
    if (value >= 60) return 30000;
    if (value >= 50) return 20000;
    if (value >= 40) return 10000;
    if (value >= 30) return 5000;
    if (value >= 20) return 2000;
    if (value >= 10) return 1000;
    return 0;
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int palette = params->iControlValue[0];
    int pitch = params->iControlValue[1];
    int color_speed = params->iControlValue[2];
    DWORD delay = delay_ms(params->iControlValue[3]);
    DWORD now = GetTickCount();
    double elapsed;
    int center_x;
    int center_y;
    int maximum;
    int arm;

    adm_canvas_fit(params, 1280, 720, &width, &height);
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (palette < 0 || palette > 2) palette = 2;
    if (pitch < 1) pitch = 1;
    if (pitch > 100) pitch = 100;
    if (color_speed < 0) color_speed = 0;
    if (color_speed > 100) color_speed = 100;
    if (g_animation_started == 0) g_animation_started = now;
    elapsed = (double)(now - g_animation_started) / 1000.0;
    if (g_canvas.frame == 0 || delay == 0 || now - g_scene_started >= delay) {
        g_scene++;
        g_scene_started = now;
        g_random_stride = (unsigned)adm_random_below(&g_canvas, 8) * 2u + 1u;
        g_random_offset = (unsigned)adm_random_below(&g_canvas, 16);
    }
    adm_canvas_clear(&g_canvas, 0);
    center_x = width / 2;
    center_y = height / 2;
    maximum = (width < height ? width : height) * 47 / 100;
    for (arm = 0; arm < 5; arm++) {
        int step;
        int old_x = center_x;
        int old_y = center_y;
        for (step = 1; step <= 420; step++) {
            double radius = (double)maximum * step / 420.0;
            double angle = (double)arm * 1.2566370614359172 +
                           (double)step * (0.012 + pitch * 0.00045) +
                           elapsed * 0.54 + g_scene * 0.31;
            int x = center_x + (int)(cos(angle) * radius);
            int y = center_y + (int)(sin(angle) * radius);
            unsigned color = palette == 0 ? (unsigned)step / 24u :
                             palette == 1 ? (unsigned)step * g_random_stride +
                                            arm * 11u + g_random_offset :
                                            (unsigned)(step / 12 + arm * 3);
            color += (unsigned)(elapsed * (double)color_speed * 1.875);
            adm_line(&g_canvas, old_x, old_y, x, y, adm_color(color));
            old_x = x;
            old_y = y;
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
        adm_canvas_release(&g_canvas); g_scene_started = 0; g_scene = 0;
        g_animation_started = 0;
        g_random_stride = 1; g_random_offset = 0;
        adm_seed(&g_canvas, 0x56455254u ^ GetTickCount()); return AD_OK;
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