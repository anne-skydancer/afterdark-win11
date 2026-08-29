/* Independent PE32 rewrite of the Classic Snake module. */

#include <windows.h>

#include "admkit.h"

#define MAX_GRID 44
#define CELLS (MAX_GRID * MAX_GRID)
#define NORTH 1
#define EAST  2
#define SOUTH 4
#define WEST  8

static ADM_CANVAS g_canvas;
static unsigned char g_walls[CELLS];
static unsigned char g_seen[CELLS];
static short g_parent[CELLS];
static short g_path[CELLS];
static int g_grid;
static int g_path_length;
static int g_reveal;
static DWORD g_last_step;
static DWORD g_completed;

static int complexity_level(int value)
{
    if (value <= 0) return 1;
    if (value >= 90) return 9;
    return value / 10;
}

static DWORD pause_ms(int value)
{
    if (value >= 98) return 60000;
    if (value >= 84) return 30000;
    if (value >= 70) return 15000;
    if (value >= 56) return 10000;
    if (value >= 42) return 5000;
    if (value >= 28) return 3000;
    if (value >= 14) return 1000;
    return 0;
}

static void generate_maze(void)
{
    short stack[CELLS];
    int top = 0;
    int current = 0;
    int total = g_grid * g_grid;
    int index;
    for (index = 0; index < total; index++) {
        g_walls[index] = NORTH | EAST | SOUTH | WEST;
        g_seen[index] = 0;
    }
    g_seen[0] = 1;
    stack[top++] = 0;
    while (top > 0) {
        int choices[4];
        int directions[4];
        int count = 0;
        int x;
        int y;
        current = stack[top - 1];
        x = current % g_grid;
        y = current / g_grid;
        if (y > 0 && !g_seen[current - g_grid]) {
            choices[count] = current - g_grid; directions[count++] = NORTH;
        }
        if (x + 1 < g_grid && !g_seen[current + 1]) {
            choices[count] = current + 1; directions[count++] = EAST;
        }
        if (y + 1 < g_grid && !g_seen[current + g_grid]) {
            choices[count] = current + g_grid; directions[count++] = SOUTH;
        }
        if (x > 0 && !g_seen[current - 1]) {
            choices[count] = current - 1; directions[count++] = WEST;
        }
        if (count == 0) { top--; continue; }
        index = adm_random_below(&g_canvas, count);
        {
            int next = choices[index];
            int direction = directions[index];
            int opposite = direction == NORTH ? SOUTH : direction == EAST ? WEST :
                           direction == SOUTH ? NORTH : EAST;
            g_walls[current] &= (unsigned char)~direction;
            g_walls[next] &= (unsigned char)~opposite;
            g_seen[next] = 1;
            stack[top++] = (short)next;
        }
    }

    {
        short queue[CELLS];
        int head = 0, tail = 0;
        int target = total - 1;
        for (index = 0; index < total; index++) g_parent[index] = -1;
        queue[tail++] = 0; g_parent[0] = 0;
        while (head < tail) {
            int cell = queue[head++];
            int neighbors[4] = { cell - g_grid, cell + 1, cell + g_grid, cell - 1 };
            int bits[4] = { NORTH, EAST, SOUTH, WEST };
            int direction;
            if (cell == target) break;
            for (direction = 0; direction < 4; direction++) {
                int next = neighbors[direction];
                if (!(g_walls[cell] & bits[direction]) && next >= 0 &&
                    next < total && g_parent[next] < 0) {
                    g_parent[next] = (short)cell;
                    queue[tail++] = (short)next;
                }
            }
        }
        g_path_length = 0;
        current = target;
        while (current != 0 && g_path_length < CELLS) {
            g_path[g_path_length++] = (short)current;
            current = g_parent[current];
        }
        g_path[g_path_length++] = 0;
        for (index = 0; index < g_path_length / 2; index++) {
            short swap = g_path[index];
            g_path[index] = g_path[g_path_length - 1 - index];
            g_path[g_path_length - 1 - index] = swap;
        }
    }
    g_reveal = 1;
    g_last_step = GetTickCount();
    g_completed = 0;
}

static void draw_maze(void)
{
    int cell = g_canvas.width / g_grid;
    int vertical = g_canvas.height / g_grid;
    int origin_x;
    int origin_y;
    int index;
    if (vertical < cell) cell = vertical;
    if (cell < 1) cell = 1;
    origin_x = (g_canvas.width - cell * g_grid) / 2;
    origin_y = (g_canvas.height - cell * g_grid) / 2;
    adm_canvas_clear(&g_canvas, 0);
    for (index = 0; index < g_grid * g_grid; index++) {
        int x = origin_x + (index % g_grid) * cell;
        int y = origin_y + (index / g_grid) * cell;
        if (g_walls[index] & NORTH) adm_line(&g_canvas, x, y, x + cell, y, adm_color(6));
        if (g_walls[index] & WEST) adm_line(&g_canvas, x, y, x, y + cell, adm_color(6));
        if (index % g_grid == g_grid - 1 && (g_walls[index] & EAST))
            adm_line(&g_canvas, x + cell, y, x + cell, y + cell, adm_color(6));
        if (index / g_grid == g_grid - 1 && (g_walls[index] & SOUTH))
            adm_line(&g_canvas, x, y + cell, x + cell, y + cell, adm_color(6));
    }
    for (index = 1; index < g_reveal && index < g_path_length; index++) {
        int previous = g_path[index - 1];
        int current = g_path[index];
        int x0 = origin_x + (previous % g_grid) * cell + cell / 2;
        int y0 = origin_y + (previous / g_grid) * cell + cell / 2;
        int x1 = origin_x + (current % g_grid) * cell + cell / 2;
        int y1 = origin_y + (current / g_grid) * cell + cell / 2;
        adm_line(&g_canvas, x0, y0, x1, y1, adm_color(2 + (unsigned)index / 8u));
    }
}

static int render_frame(AD_MODULE32 *params)
{
    int width;
    int height;
    int speed = params->iControlValue[0];
    int desired_grid = 8 + complexity_level(params->iControlValue[1]) * 4;
    DWORD pause = pause_ms(params->iControlValue[2]);
    DWORD now = GetTickCount();
    DWORD interval;
    adm_canvas_fit(params, 1280, 720, &width, &height);
    if (!adm_canvas_resize(&g_canvas, width, height)) return 0;
    if (speed < 1) speed = 1;
    if (speed > 9) speed = 9;
    interval = (DWORD)(110 - speed * 11);
    if (g_grid != desired_grid) { g_grid = desired_grid; generate_maze(); }
    if (g_reveal < g_path_length && now - g_last_step >= interval) {
        unsigned steps = (now - g_last_step) / interval;
        g_reveal += (int)steps;
        if (g_reveal > g_path_length) g_reveal = g_path_length;
        g_last_step += steps * interval;
        if (g_reveal == g_path_length) g_completed = now;
    } else if (g_reveal >= g_path_length && now - g_completed >= pause) {
        generate_maze();
    }
    draw_maze();
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
        adm_canvas_release(&g_canvas); adm_seed(&g_canvas, 0x534E414Bu ^ GetTickCount());
        g_grid = 0; return AD_OK;
    case AD_MSG_PREINITIALIZE: return AD_OK;
    case AD_MSG_BLANK:
    case AD_MSG_DRAWFRAME: return render_frame(params) ? AD_OK : 1;
    case AD_MSG_PAINT:
        if (!g_canvas.has_frame) return render_frame(params) ? AD_OK : 1;
        adm_canvas_present(&g_canvas, params); return AD_OK;
    case AD_MSG_CLOSE:
    case AD_MSG_MODULEDESELECTED: adm_canvas_release(&g_canvas); g_grid = 0; return AD_OK;
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