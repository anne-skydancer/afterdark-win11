#include "admkit.h"

#include <math.h>
#include <stdlib.h>

static const uint32_t g_colors[] = {
    0x00F2B33D, 0x005BC98B, 0x004DA3FF, 0x00F06A8A,
    0x00D98CFF, 0x00F4F4F6, 0x0058D6E8, 0x00F09A55,
    0x00FFF176, 0x0078D4A7, 0x006CB6FF, 0x00FF8AA4,
    0x00E4A7FF, 0x00FFFFFF, 0x007CE7F5, 0x00FFB570,
};

void adm_canvas_release(ADM_CANVAS *canvas)
{
    if (canvas->pixels) free(canvas->pixels);
    ZeroMemory(canvas, sizeof(*canvas));
}

int adm_canvas_resize(ADM_CANVAS *canvas, int width, int height)
{
    size_t count;
    uint32_t *pixels;

    if (width < 1 || height < 1 || width > 4096 || height > 4096) return 0;
    if (canvas->pixels && width == canvas->width && height == canvas->height)
        return 1;

    count = (size_t)width * (size_t)height;
    pixels = (uint32_t *)realloc(canvas->pixels, count * sizeof(*pixels));
    if (!pixels) return 0;
    canvas->pixels = pixels;
    canvas->width = width;
    canvas->height = height;
    canvas->frame = 0;
    canvas->has_frame = 0;
    adm_canvas_clear(canvas, 0);
    return 1;
}

void adm_canvas_clear(ADM_CANVAS *canvas, uint32_t color)
{
    size_t index;
    size_t count = (size_t)canvas->width * (size_t)canvas->height;
    for (index = 0; index < count; index++) canvas->pixels[index] = color;
}

void adm_canvas_fade(ADM_CANVAS *canvas, unsigned numerator, unsigned denominator)
{
    size_t index;
    size_t count;
    if (!canvas->pixels || denominator == 0) return;
    count = (size_t)canvas->width * (size_t)canvas->height;
    for (index = 0; index < count; index++) {
        uint32_t color = canvas->pixels[index];
        unsigned red = ((color >> 16) & 0xFFu) * numerator / denominator;
        unsigned green = ((color >> 8) & 0xFFu) * numerator / denominator;
        unsigned blue = (color & 0xFFu) * numerator / denominator;
        canvas->pixels[index] = (red << 16) | (green << 8) | blue;
    }
}

void adm_canvas_present(const ADM_CANVAS *canvas, const AD_MODULE32 *params)
{
    BITMAPINFO info;
    if (!canvas->pixels || !params->hDC) return;

    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = canvas->width;
    info.bmiHeader.biHeight = -canvas->height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(params->hDC, 0, 0, canvas->width, canvas->height,
                  0, 0, canvas->width, canvas->height, canvas->pixels, &info,
                  DIB_RGB_COLORS, SRCCOPY);
}

void adm_seed(ADM_CANVAS *canvas, uint32_t seed)
{
    canvas->random = seed ? seed : 0x41444D4Bu;
}

uint32_t adm_random(ADM_CANVAS *canvas)
{
    uint32_t value = canvas->random;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    canvas->random = value;
    return value;
}

int adm_random_below(ADM_CANVAS *canvas, int limit)
{
    return limit > 0 ? (int)(adm_random(canvas) % (uint32_t)limit) : 0;
}

uint32_t adm_color(unsigned index)
{
    return g_colors[index % (sizeof(g_colors) / sizeof(g_colors[0]))];
}

void adm_put_pixel(ADM_CANVAS *canvas, int x, int y, uint32_t color)
{
    if ((unsigned)x < (unsigned)canvas->width &&
        (unsigned)y < (unsigned)canvas->height)
        canvas->pixels[(size_t)y * (size_t)canvas->width + (size_t)x] = color;
}

void adm_line(ADM_CANVAS *canvas, int x0, int y0, int x1, int y1,
              uint32_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        int twice;
        adm_put_pixel(canvas, x0, y0, color);
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

void adm_rectangle(ADM_CANVAS *canvas, int left, int top, int right, int bottom,
                   uint32_t color)
{
    adm_line(canvas, left, top, right, top, color);
    adm_line(canvas, right, top, right, bottom, color);
    adm_line(canvas, right, bottom, left, bottom, color);
    adm_line(canvas, left, bottom, left, top, color);
}

void adm_round_rectangle(ADM_CANVAS *canvas, int left, int top, int right,
                         int bottom, int radius, uint32_t color)
{
    int step;
    int previous_x;
    int previous_y;
    int center_x;
    int center_y;

    if (radius < 1) {
        adm_rectangle(canvas, left, top, right, bottom, color);
        return;
    }
    if (radius * 2 > right - left) radius = (right - left) / 2;
    if (radius * 2 > bottom - top) radius = (bottom - top) / 2;
    adm_line(canvas, left + radius, top, right - radius, top, color);
    adm_line(canvas, right, top + radius, right, bottom - radius, color);
    adm_line(canvas, right - radius, bottom, left + radius, bottom, color);
    adm_line(canvas, left, bottom - radius, left, top + radius, color);

    for (step = 0; step < 4; step++) {
        int point;
        double start = (double)step * 1.57079632679489661923;
        center_x = (step == 0 || step == 3) ? right - radius : left + radius;
        center_y = step < 2 ? bottom - radius : top + radius;
        previous_x = center_x + (int)(cos(start) * radius);
        previous_y = center_y + (int)(sin(start) * radius);
        for (point = 1; point <= 12; point++) {
            double angle = start + (double)point * 1.57079632679489661923 / 12.0;
            int x = center_x + (int)(cos(angle) * radius);
            int y = center_y + (int)(sin(angle) * radius);
            adm_line(canvas, previous_x, previous_y, x, y, color);
            previous_x = x;
            previous_y = y;
        }
    }
}