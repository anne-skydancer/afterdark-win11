/*
 * ad_module32.h -- the After Dark 4 (32-bit) module ABI.
 *
 * Recovered by disassembling AFTERDAR.SCR, TOASTERS.AD and BADDOG.AD.
 * See docs/ABI.md for the evidence behind every field and constant.
 *
 * This is NOT the After Dark 3 ABI. AD3 used
 *     int FAR PASCAL Module(int msg, HDC hdc, HANDLE hADSystem);
 * AD4 passes a single pointer to the block below, with the message inside it,
 * and renumbered the messages. A host built to the published AD3 constants
 * will send the wrong ones.
 *
 * Contains no Berkeley Systems code. Interface information only.
 */

#ifndef AD_MODULE32_H
#define AD_MODULE32_H

/* Normally just pulls in the Win32 types. Define AD_MODULE32_NO_WINDOWS_H and
 * supply DWORD/HWND/HINSTANCE/HANDLE/RECT yourself to compile this header on a
 * non-Windows host (the layout test does exactly that). */
#ifndef AD_MODULE32_NO_WINDOWS_H
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* __stdcall is the real convention; it simply has no meaning off x86 Windows,
 * where this header is still useful for layout checks and tooling. */
#if defined(_WIN32) || defined(__i386__)
#  define AD_STDCALL __stdcall
#else
#  define AD_STDCALL
#endif

#pragma pack(push, 1)

/* The parameter block. Exactly 348 bytes; cbSize must say so. */
typedef struct tagAD_MODULE32
{
    /* +0x000 */ DWORD     cbSize;            /* = AD_MODULE32_SIZE           */
    /* +0x004 */ DWORD     dwFlags;           /* AD_FLAG_* below, host -> module */
    /* +0x008 */ DWORD     dwModuleFlags;     /* module -> host; re-read after
                                                 every call                   */
    /* +0x00C */ DWORD     dwReserved00C;
    /* +0x010 */ HWND      hWnd;              /* window the module draws into */
    /* +0x014 */ HINSTANCE hModule;           /* HMODULE of the .AD itself    */
    /* +0x018 */ HDC       hDC;               /* device context the module draws
                                                 into. The first field every
                                                 module reads; passed straight
                                                 to SetViewportOrgEx. This is
                                                 AD3's hDrawDC argument, moved
                                                 into the block.              */
    /* +0x01C */ RECT      rcDemo;            /* preview/demo rect (inferred) */
    /* +0x02C */ RECT      rcClient;          /* GetClientRect(hWnd, &rcClient) */
    /* +0x03C */ DWORD     dwReserved03C;
    /* +0x040 */ int       iControlValue[4];  /* the four module settings     */
    /* +0x050 */ char      szMessage[260];    /* status/error text set by the
                                                 module; host shows "path: text" */
    /* +0x154 */ DWORD     dwMessage;         /* AD_MSG_*                     */
    /* +0x158 */ DWORD     dwParam;           /* some handlers use only the low
                                                 WORD or low BYTE             */
} AD_MODULE32;

#pragma pack(pop)

#define AD_MODULE32_SIZE  0x15C

/* Fail fast if the compiler pads this differently than the engine expects. */
#ifdef __cplusplus
static_assert(sizeof(AD_MODULE32) == AD_MODULE32_SIZE,
              "AD_MODULE32 must be exactly 0x15C bytes");
#else
typedef char ad_module32_size_check[
    (sizeof(AD_MODULE32) == AD_MODULE32_SIZE) ? 1 : -1];
#endif

/* ---- dwFlags: host -> module -------------------------------------------- */
#define AD_FLAG_PALETTE     0x00000001u  /* display supports palettes
                                            (GetDeviceCaps RASTERCAPS/RC_PALETTE) */
#define AD_FLAG_SOUND       0x00000012u  /* original host sets bits 1 and 4
                                            whenever MuteSound is false     */
#define AD_FLAG_MODE_1      0x00000004u  /* bits 2/3 select the run mode the  */
#define AD_FLAG_MODE_2      0x00000008u  /* module reads at entry             */

/* ---- messages: value of dwMessage ---------------------------------------
 * Valid range is 0..30; the module's dispatch table is bounded at 30.
 * Values not listed fall through to the module's default case.
 */
#define AD_MSG_MODULESELECTED     0   /* DoSelected                          */
#define AD_MSG_MODULEDESELECTED   1   /* DoDeselected                        */
#define AD_MSG_PREINITIALIZE      2   /* DoPreInitialize                     */
#define AD_MSG_BLANK              3   /* DoBlankScreen                       */
#define AD_MSG_DRAWFRAME          4   /* DoDrawFrame; first call also blanks */
#define AD_MSG_CLOSE              5   /* teardown                            */
#define AD_MSG_BUTTON             6   /* dwParam & 0xFFFF = control index    */
#define AD_MSG_KEYDOWN            7   /* dwParam & 0xFF   = key              */
#define AD_MSG_KEYUP              8   /* dwParam & 0xFF   = key              */
#define AD_MSG_PAINT              9   /* DoPaint                             */
#define AD_MSG_LBUTTONDOWN       21
#define AD_MSG_LBUTTONHELD       24
#define AD_MSG_LBUTTONUP         27
#define AD_MSG_MOUSEMOVE         30
#define AD_MSG_MAX               30

/* ---- return values ------------------------------------------------------ */
#define AD_OK                     0
#define AD_RESTART_ME             3   /* host must re-send PREINITIALIZE then
                                         BLANK, then resume the frame loop   */

/* ---- the entry point ----------------------------------------------------
 * Exported as "_Module@4" (MSVC-decorated) or, for the Borland-built modules
 * that actually shipped, undecorated as "Module". Resolve the decorated name
 * first and fall back, exactly as AFTERDAR.SCR does.
 */
typedef int (AD_STDCALL *AD_MODULEPROC)(AD_MODULE32 *params);

#define AD_ENTRY_DECORATED    "_Module@4"
#define AD_ENTRY_UNDECORATED  "Module"

/* The engine runtime every AD4 module imports 150-300 functions from. It must
 * be loaded, from the user's own installation, before any module will bind. */
#define AD_ENGINE_DLL         "ADXPL510.DLL"

#ifdef __cplusplus
}
#endif

#endif /* AD_MODULE32_H */
