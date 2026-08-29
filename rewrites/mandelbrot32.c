/*
 * MANDEL32.AD -- independent 32-bit Mandelbrot module for After Dark Studio.
 *
 * Implements the recovered AD_MODULE32 interoperability ABI. It contains no
 * Berkeley Systems code or assets; the original module's two setting labels
 * are reproduced as interface information by the generated resources.
 */

#include <windows.h>
#include <stdint.h>
#include <stdlib.h>

#include "../include/ad_module32.h"

static uint32_t *g_pixels;
static int g_width;
static int g_height;
static DWORD g_last_render;
static unsigned g_scene;
static int g_has_frame;

static void release_pixels(void)
{
    if (g_pixels) free(g_pixels);
    g_pixels = NULL;
    g_width = 0;
    g_height = 0;
    g_has_frame = 0;
}

static int ensure_pixels(int width, int height)
{
    size_t count;
    uint32_t *pixels;

    if (width < 1 || height < 1 || width > 4096 || height > 4096) return 0;
    if (g_pixels && width == g_width && height == g_height) return 1;

    count = (size_t)width * (size_t)height;
    pixels = (uint32_t *)realloc(g_pixels, count * sizeof(*pixels));
    if (!pixels) return 0;
    g_pixels = pixels;
    g_width = width;
    g_height = height;
    return 1;
}

static uint32_t color_for(int iterations, int limit, int palette)
{
    int t, r, g, b;
    if (iterations >= limit) return 0;

    t = iterations * 255 / limit;
    switch (palette) {
    case 0: /* Earth */
        r = 40 + (t * 5 / 8);
        g = 24 + (t * 3 / 4);
        b = 12 + (t / 5);
        break;
    case 1: /* Air */
        r = 70 + (t * 3 / 4);
        g = 110 + (t * 9 / 16);
        b = 150 + (t * 2 / 5);
        break;
    case 2: /* Fire */
        r = 90 + (t * 13 / 20);
        g = t * t / 255;
        b = t / 8;
        break;
    default: /* Water */
        r = t / 8;
        g = 35 + (t * 3 / 5);
        b = 90 + (t * 13 / 20);
        break;
    }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void present(const AD_MODULE32 *params)
{
    BITMAPINFO info;
    if (!g_pixels || !params->hDC) return;

    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = g_width;
    info.bmiHeader.biHeight = -g_height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(params->hDC, 0, 0, g_width, g_height,
                  0, 0, g_width, g_height, g_pixels, &info,
                  DIB_RGB_COLORS, SRCCOPY);
}

static int render_scene(AD_MODULE32 *params)
{
    static const double centers[][3] = {
        { -0.5000000000,  0.0000000000, 1.55 },
        { -0.7436438870,  0.1318259042, 0.018 },
        { -1.2506600000,  0.0201200000, 0.045 },
        { -0.1607013500,  1.0375665000, 0.022 },
        {  0.2850000000,  0.0100000000, 0.30 },
    };
    const int limit = 96;
    int width = params->rcClient.right - params->rcClient.left;
    int height = params->rcClient.bottom - params->rcClient.top;
    int palette = params->iControlValue[1];
    unsigned view = g_scene % (sizeof(centers) / sizeof(centers[0]));
    double center_x = centers[view][0];
    double center_y = centers[view][1];
    double span_y = centers[view][2];
    double span_x;
    int x, y;

    if (width < 2 || height < 2) return 0;
    if (!ensure_pixels(width, height)) return 0;
    if (palette < 0 || palette > 4) palette = 3;
    if (palette == 4) palette = (int)((g_scene * 1103515245u + 12345u) % 4u);
    span_x = span_y * (double)width / (double)height;

    for (y = 0; y < height; y++) {
        double cy = center_y + (((double)y / (double)(height - 1)) - 0.5) * span_y;
        for (x = 0; x < width; x++) {
            double cx = center_x + (((double)x / (double)(width - 1)) - 0.5) * span_x;
            double zx = 0.0, zy = 0.0;
            int i;
            for (i = 0; i < limit && zx * zx + zy * zy <= 4.0; i++) {
                double next_x = zx * zx - zy * zy + cx;
                zy = 2.0 * zx * zy + cy;
                zx = next_x;
            }
            g_pixels[(size_t)y * (size_t)width + (size_t)x] = color_for(i, limit, palette);
        }
    }

    g_scene++;
    g_last_render = GetTickCount();
    g_has_frame = 1;
    present(params);
    return 1;
}

static DWORD delay_milliseconds(int value)
{
    switch (value) {
    case 20: return 5000;
    case 40: return 15000;
    case 60: return 30000;
    case 80: return 60000;
    default: return 250;
    }
}

__declspec(dllexport) int AD_STDCALL Module(AD_MODULE32 *params)
{
    if (!params || params->cbSize < AD_MODULE32_SIZE) return 1;

    switch (params->dwMessage) {
    case AD_MSG_MODULESELECTED:
        g_scene = 0;
        g_last_render = 0;
        g_has_frame = 0;
        return AD_OK;

    case AD_MSG_PREINITIALIZE:
        g_last_render = 0;
        return AD_OK;

    case AD_MSG_BLANK:
        return render_scene(params) ? AD_OK : 1;

    case AD_MSG_DRAWFRAME:
        if (!g_has_frame || GetTickCount() - g_last_render >=
                            delay_milliseconds(params->iControlValue[0]))
            return render_scene(params) ? AD_OK : 1;
        return AD_OK;

    case AD_MSG_PAINT:
        if (!g_has_frame) return render_scene(params) ? AD_OK : 1;
        present(params);
        return AD_OK;

    case AD_MSG_CLOSE:
    case AD_MSG_MODULEDESELECTED:
        release_pixels();
        return AD_OK;

    default:
        return AD_OK;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    if (reason == DLL_PROCESS_DETACH) release_pixels();
    return TRUE;
}
