/*
 * SHAPES32.AD -- independent 32-bit Shapes module for After Dark Studio.
 *
 * Implements the recovered AD_MODULE32 interoperability ABI. It contains no
 * Berkeley Systems code or assets; the original module's two setting labels
 * are reproduced as interface information by the generated resources.
 */

#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "../include/ad_module32.h"

static uint32_t *g_pixels;
static int g_width;
static int g_height;
static unsigned g_shape_count;
static uint32_t g_random = 0x53485053u;
static int g_has_frame;

static void release_pixels(void)
{
    if (g_pixels) free(g_pixels);
    g_pixels = NULL;
    g_width = 0;
    g_height = 0;
    g_shape_count = 0;
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
    ZeroMemory(g_pixels, count * sizeof(*g_pixels));
    g_shape_count = 0;
    g_has_frame = 0;
    return 1;
}

static uint32_t next_random(void)
{
    uint32_t value = g_random;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    g_random = value;
    return value;
}

static int random_below(int limit)
{
    return limit > 0 ? (int)(next_random() % (uint32_t)limit) : 0;
}

static void clear_canvas(void)
{
    if (g_pixels)
        ZeroMemory(g_pixels, (size_t)g_width * (size_t)g_height * sizeof(*g_pixels));
    g_shape_count = 0;
}

static void put_pixel(int x, int y, uint32_t color)
{
    if ((unsigned)x < (unsigned)g_width && (unsigned)y < (unsigned)g_height)
        g_pixels[(size_t)y * (size_t)g_width + (size_t)x] = color;
}

static void draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        int twice;
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void draw_rectangle(int left, int top, int right, int bottom, uint32_t color)
{
    draw_line(left, top, right, top, color);
    draw_line(right, top, right, bottom, color);
    draw_line(right, bottom, left, bottom, color);
    draw_line(left, bottom, left, top, color);
}

static void draw_ellipse(int center_x, int center_y, int radius_x, int radius_y,
                         uint32_t color)
{
    int x;
    int previous_x = center_x + radius_x;
    int previous_y = center_y;

    for (x = 1; x <= 96; x++) {
        double angle = (double)x * 6.28318530717958647692 / 96.0;
        int next_x = center_x + (int)(cos(angle) * radius_x);
        int next_y = center_y + (int)(sin(angle) * radius_y);
        draw_line(previous_x, previous_y, next_x, next_y, color);
        previous_x = next_x;
        previous_y = next_y;
    }
}

static uint32_t shape_color(int enabled)
{
    static const uint32_t colors[] = {
        0x00F2B33D, 0x005BC98B, 0x004DA3FF, 0x00F06A8A,
        0x00D98CFF, 0x00F4F4F6, 0x0058D6E8, 0x00F09A55,
    };
    if (!enabled) return 0x00F4F4F6;
    return colors[random_below((int)(sizeof(colors) / sizeof(colors[0])))];
}

static void draw_shape(int colored)
{
    int max_size = g_width < g_height ? g_width : g_height;
    int radius_x;
    int radius_y;
    int center_x;
    int center_y;
    uint32_t color;

    if (max_size < 8) return;
    radius_x = 4 + random_below(max_size / 3);
    radius_y = 4 + random_below(max_size / 3);
    center_x = random_below(g_width);
    center_y = random_below(g_height);
    color = shape_color(colored);

    switch (random_below(5)) {
    case 0:
        draw_rectangle(center_x - radius_x, center_y - radius_y,
                       center_x + radius_x, center_y + radius_y, color);
        break;
    case 1:
        draw_ellipse(center_x, center_y, radius_x, radius_y, color);
        break;
    case 2:
        draw_line(center_x, center_y - radius_y,
                  center_x + radius_x, center_y + radius_y, color);
        draw_line(center_x + radius_x, center_y + radius_y,
                  center_x - radius_x, center_y + radius_y, color);
        draw_line(center_x - radius_x, center_y + radius_y,
                  center_x, center_y - radius_y, color);
        break;
    case 3:
        draw_line(center_x, center_y - radius_y,
                  center_x + radius_x, center_y, color);
        draw_line(center_x + radius_x, center_y,
                  center_x, center_y + radius_y, color);
        draw_line(center_x, center_y + radius_y,
                  center_x - radius_x, center_y, color);
        draw_line(center_x - radius_x, center_y,
                  center_x, center_y - radius_y, color);
        break;
    default:
        draw_line(center_x - radius_x, center_y - radius_y,
                  center_x + radius_x, center_y + radius_y, color);
        draw_line(center_x + radius_x, center_y - radius_y,
                  center_x - radius_x, center_y + radius_y, color);
        break;
    }
    g_shape_count++;
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

static int render_frame(AD_MODULE32 *params)
{
    int width = params->rcClient.right - params->rcClient.left;
    int height = params->rcClient.bottom - params->rcClient.top;
    unsigned clear_after = params->iControlValue[0] ? 40u : 320u;

    if (!ensure_pixels(width, height)) return 0;
    if (g_shape_count >= clear_after) clear_canvas();
    draw_shape(params->iControlValue[1] != 0);
    g_has_frame = 1;
    present(params);
    return 1;
}

__declspec(dllexport) int AD_STDCALL Module(AD_MODULE32 *params)
{
    if (!params || params->cbSize < AD_MODULE32_SIZE) return 1;

    switch (params->dwMessage) {
    case AD_MSG_MODULESELECTED:
        release_pixels();
        g_random = 0x53485053u ^ GetTickCount();
        if (g_random == 0) g_random = 0x53485053u;
        return AD_OK;

    case AD_MSG_PREINITIALIZE:
        return AD_OK;

    case AD_MSG_BLANK:
        if (!ensure_pixels(params->rcClient.right - params->rcClient.left,
                           params->rcClient.bottom - params->rcClient.top))
            return 1;
        clear_canvas();
        return render_frame(params) ? AD_OK : 1;

    case AD_MSG_DRAWFRAME:
        return render_frame(params) ? AD_OK : 1;

    case AD_MSG_PAINT:
        if (!g_has_frame) return render_frame(params) ? AD_OK : 1;
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