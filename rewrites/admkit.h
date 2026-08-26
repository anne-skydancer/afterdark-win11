#ifndef AFTERDARK_ADMKIT_H
#define AFTERDARK_ADMKIT_H

#include <windows.h>
#include <stdint.h>

#include "../include/ad_module32.h"

typedef struct ADM_CANVAS {
    uint32_t *pixels;
    int width;
    int height;
    uint32_t random;
    unsigned frame;
    int has_frame;
} ADM_CANVAS;

void adm_canvas_release(ADM_CANVAS *canvas);
int adm_canvas_resize(ADM_CANVAS *canvas, int width, int height);
void adm_canvas_fit(const AD_MODULE32 *params, int maximum_width,
                    int maximum_height, int *width, int *height);
void adm_canvas_clear(ADM_CANVAS *canvas, uint32_t color);
void adm_canvas_fade(ADM_CANVAS *canvas, unsigned numerator, unsigned denominator);
void adm_canvas_present(const ADM_CANVAS *canvas, const AD_MODULE32 *params);
void adm_seed(ADM_CANVAS *canvas, uint32_t seed);
uint32_t adm_random(ADM_CANVAS *canvas);
int adm_random_below(ADM_CANVAS *canvas, int limit);
uint32_t adm_color(unsigned index);
void adm_put_pixel(ADM_CANVAS *canvas, int x, int y, uint32_t color);
void adm_line(ADM_CANVAS *canvas, int x0, int y0, int x1, int y1,
              uint32_t color);
void adm_rectangle(ADM_CANVAS *canvas, int left, int top, int right, int bottom,
                   uint32_t color);
void adm_round_rectangle(ADM_CANVAS *canvas, int left, int top, int right,
                         int bottom, int radius, uint32_t color);
void adm_ellipse(ADM_CANVAS *canvas, int center_x, int center_y,
                 int radius_x, int radius_y, uint32_t color);
void adm_filled_triangle(ADM_CANVAS *canvas, int x0, int y0, int x1, int y1,
                         int x2, int y2, uint32_t fill, uint32_t outline);

#endif