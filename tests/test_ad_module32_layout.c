/*
 * Layout regression test for include/ad_module32.h.
 *
 * The offsets asserted here are the ones recovered from AFTERDAR.SCR and the
 * shipped modules (see docs/ABI.md). If this test fails, a host built against
 * the header will hand modules a block they will misread -- silently.
 *
 * It models 32-bit Windows types explicitly so it runs anywhere:
 *      cc -o test tests/test_ad_module32_layout.c && ./test
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 32-bit Windows types -- the target ABI, regardless of the build host. */
typedef uint32_t DWORD;
typedef uint32_t HWND;
typedef uint32_t HINSTANCE;
typedef uint32_t HANDLE;
typedef uint32_t HDC;
typedef int32_t  LONG;
typedef struct { LONG left, top, right, bottom; } RECT;

#define AD_MODULE32_NO_WINDOWS_H
#include "../include/ad_module32.h"

static int failures;

static void check(const char *name, size_t actual, unsigned expected)
{
    int ok = (actual == (size_t)expected);
    printf("  %-16s +0x%03zX   expect +0x%03X   %s\n",
           name, actual, expected, ok ? "ok" : "MISMATCH");
    if (!ok) failures++;
}

#define CHECK(field, expected) check(#field, offsetof(AD_MODULE32, field), expected)

int main(void)
{
    printf("AD_MODULE32 layout (docs/ABI.md)\n\n");
    printf("  %-16s  0x%03zX   expect  0x%03X   %s\n\n", "sizeof",
           sizeof(AD_MODULE32), AD_MODULE32_SIZE,
           sizeof(AD_MODULE32) == AD_MODULE32_SIZE ? "ok" : "MISMATCH");
    if (sizeof(AD_MODULE32) != AD_MODULE32_SIZE) failures++;

    CHECK(cbSize,         0x000);
    CHECK(dwFlags,        0x004);
    CHECK(dwModuleFlags,  0x008);
    CHECK(hWnd,           0x010);
    CHECK(hModule,        0x014);
    CHECK(hDC,            0x018);
    CHECK(rcDemo,         0x01C);
    CHECK(rcClient,       0x02C);
    CHECK(iControlValue,  0x040);
    CHECK(szMessage,      0x050);
    CHECK(dwMessage,      0x154);
    CHECK(dwParam,        0x158);

    /* Message numbering differs from After Dark 3; guard the ones that moved. */
    if (AD_MSG_BLANK != 3 || AD_MSG_DRAWFRAME != 4 || AD_MSG_CLOSE != 5) {
        printf("\n  message constants do not match the recovered table\n");
        failures++;
    }
    if (AD_FLAG_SOUND != 0x12) {
        printf("\n  sound flag does not match the original unmuted host\n");
        failures++;
    }

    printf("\n%s\n", failures ? "FAILED" : "all offsets match the disassembly");
    return failures ? 1 : 0;
}
