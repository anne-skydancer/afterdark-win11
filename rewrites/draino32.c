/* Independent PE32 rewrite of the Classic Down the Drain module. */

#include <windows.h>
#include <math.h>

#include "admkit.h"

typedef struct PARTICLE {
    double angle;
    double radius;
    unsigned color;
} PARTICLE;

static ADM_CANVAS g_canvas;
static PARTICLE g_particles[128];
static int g_initialized;

static void reset_particle(PARTICLE *particle, double maximum_radius)
{
    particle->angle = (double)adm_random_below(&g_canvas, 6284) / 1000.0;
    particle->radius = maximum_radius *
                       (0.65 + (double)adm_random_below(&g_canvas, 351) / 1000.0);
    particle->color = (unsigned)adm_random_below(&g_canvas, 16);
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int speed_value = params->iControlValue[0];
    int direction = params->iControlValue[1];
    int show_drops = params->iControlValue[2] != 0;
    int show_drain = params->iControlValue[3] != 0;
    int center_x;
    int center_y;
    double maximum_radius;
    double speed;
    int index;
    int resized;

    adm_canvas_fit(params, 1280, 720, &width, &height);
    resized = g_canvas.width != width || g_canvas.height != height;
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    center_x = width / 2;
    center_y = height / 2;
    maximum_radius = (double)(width < height ? width : height) * 0.47;
    speed = speed_value >= 67 ? 4.5 : speed_value >= 33 ? 2.8 : 1.4;
    if (resized) g_initialized = 0;
    if (!g_initialized) {
        for (index = 0; index < 128; index++)
            reset_particle(&g_particles[index], maximum_radius);
        g_initialized = 1;
    }
    adm_canvas_fade(&g_canvas, 15, 16);
    for (index = 0; index < 128; index++) {
        PARTICLE *particle = &g_particles[index];
        double turn = direction < 33 ? 0.025 : direction >= 67 ? -0.025 : 0.0;
        int old_x = center_x + (int)(cos(particle->angle) * particle->radius);
        int old_y = center_y + (int)(sin(particle->angle) * particle->radius * 0.72);
        int x;
        int y;
        particle->angle += turn * speed;
        particle->radius -= speed;
        if (particle->radius < 4.0) reset_particle(particle, maximum_radius);
        x = center_x + (int)(cos(particle->angle) * particle->radius);
        y = center_y + (int)(sin(particle->angle) * particle->radius * 0.72);
        if (show_drops)
            adm_filled_ellipse(&g_canvas, x, y, 2, 3,
                               adm_color(particle->color));
        else
            adm_line(&g_canvas, old_x, old_y, x, y,
                     adm_color(particle->color));
    }
    if (show_drain) {
        adm_ellipse(&g_canvas, center_x, center_y, 28, 14, adm_color(14));
        adm_ellipse(&g_canvas, center_x, center_y, 18, 9, adm_color(5));
        adm_ellipse(&g_canvas, center_x, center_y, 8, 4, 0);
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
        adm_seed(&g_canvas, 0x44524149u ^ GetTickCount());
        g_initialized = 0;
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