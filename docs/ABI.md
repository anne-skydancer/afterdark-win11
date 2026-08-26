# The After Dark 4 module ABI, recovered

This was the one unknown gating the whole project ([DESIGN.md](DESIGN.md) §7).
It is now resolved. A host can call AD4 modules.

Everything below was recovered by disassembling `AFTERDAR.SCR` (the 95 KB Win32
screensaver host) and `TOASTERS.AD` / `BADDOG.AD`, and cross-checked between the
two modules. Addresses are file VAs at the default image base (`0x400000` for
all three).

**Headline result: the AD3-era signature is gone.** After Dark 3 used
`int FAR PASCAL Module(int iMessage, HDC hDrawDC, HANDLE hADSystem)` — three
arguments. After Dark 4 passes **one pointer to a 348-byte block**, with the
message inside it.

---

## 1. Entry point

```c
int __stdcall Module(AD_MODULE32 *params);
```

**Evidence.** The loader at `0x404850` does `LoadLibraryA(path)`, then:

```
404887:  push 0x4107f0            ; "_Module@4"
40488d:  call GetProcAddress
404893:  mov [edi],eax
404897:  jne 0x4048a8             ; got it -> done
404899:  push 0x4107e8            ; "Module"
4048a0:  call GetProcAddress      ; fall back to the undecorated name
```

The binary literally contains both strings, adjacent:

```
0x4107d8: "oldmod32.dll"  "Module"  "_Module@4"
```

`_Module@4` is the MSVC `__stdcall` decoration for **4 bytes of arguments** —
one 32-bit parameter. The undecorated fallback exists because Borland-built
modules (which is what shipped — the exports are Borland-mangled) export via a
`.DEF` file without decoration. `TOASTERS.AD` exports plain `Module`.

The call site confirms both the convention and the argument count:

```
402371:  lea eax,[esi+0x128]      ; &block
402377:  push eax                 ; one argument
402378:  call [ebx]               ; Module(&block)
40237a:  test eax,eax             ; ...no 'add esp,4' -> callee cleaned -> stdcall
```

`oldmod32.dll` is the 32→16 thunk used for Classic modules — the mechanism
behind the `Module3216` / `ModuleMessage3216` / `ModuleCtrlValues3216` strings,
and Win9x-only. Confirms Classic modules are unreachable on Windows 11.

---

## 2. The parameter block

The host allocates the block inline in its module-manager object at `+0x128`,
zeroes it, and stamps its own size into the first DWORD:

```
40210c:  mov ebx,0x15c
402111:  lea edi,[esi+0x128]
402117:  push ebx / push 0 / push edi
40211b:  call memset              ; memset(block, 0, 0x15C)
402131:  mov [edi],ebx            ; block->cbSize = 0x15C
```

So the block is **0x15C (348) bytes** and is **self-describing by size** — a
version field. A host should set it exactly.

### Field map

| Offset | Type | Meaning | Status |
|---|---|---|---|
| `+0x000` | DWORD | `cbSize` — always `0x15C` | **confirmed** (`0x402131`) |
| `+0x004` | DWORD | Input flags. bit0 = display supports palettes; bit1, bit4 also set by host; **bit2/bit3 select the run mode** the module reads | **confirmed** (`0x402163`–`0x402190`, read at `0x41e509`) |
| `+0x008` | DWORD | Output flags, module → host. Host re-reads after every call and splits bits 0/1/2 into its own state | **confirmed** (`0x402415`–`0x402437`) |
| `+0x010` | HWND | Target window | **confirmed** — passed as arg 1 to `GetClientRect` at `0x402141` |
| `+0x014` | HINSTANCE | `HMODULE` of the loaded `.AD` | **confirmed** (`0x40235b`, from `LoadLibrary`) |
| `+0x018` | handle | Host/engine context. **The first thing every module reads** | **confirmed** (`0x41e4df`, `0x428073`) |
| `+0x01C` | RECT | Copied verbatim from a host global (`0x4130c8`); the preview/demo rect | confirmed copy; **name inferred** |
| `+0x02C` | RECT | Client rect — `GetClientRect(hwnd, block+0x2C)` | **confirmed** (`0x402141`) |
| `+0x03C` | DWORD | Set from a caller argument | confirmed write; meaning unknown |
| `+0x040` | int[4] | **`iControlValue[4]`** — the four module settings, copied from a caller-supplied array | **confirmed** (`0x40214b`–`0x402161`, 4-iteration copy loop) |
| `+0x050` | char[260] | Status/error string. If non-empty the host formats `"%s: %s"` with the module path | **confirmed** use (`0x40244d`); length inferred from the gap to `+0x154` |
| `+0x154` | DWORD | **Message** | **confirmed** (`0x4023f0` writes it; `0x41e5d3` reads it) |
| `+0x158` | DWORD | **Message parameter.** Several handlers read only the low WORD or low BYTE | **confirmed** (`0x4023f6`, `0x41e6e0`) |

Bytes `0x54`–`0x153` beyond the string buffer are not individually mapped; the
host leaves them zero, so a host that zeroes the block is safe.

`+0x040` being the control array is the payoff for configuration: the values
[extracted from module resources](../tools/ad_extract.py) go straight into
`iControlValue[slot]`, exactly as designed.

---

## 3. Messages

The module dispatches on `+0x154` through a two-level jump table bounded at
**30** (`cmp eax,0x1e`). Identical code in `TOASTERS.AD` (`0x41e5d3`) and
`BADDOG.AD` (`0x428167`) — this is SDK boilerplate, not per-module.

Each number was identified by resolving the handler's call target through the
module's import thunks into **named** `ADXPL510.DLL` exports:

| Msg | Name | Module handler resolves to |
|---:|---|---|
| 0 | `AD_MODULESELECTED` | `DoSelected` |
| 1 | `AD_MODULEDESELECTED` | `DoDeselected` |
| 2 | `AD_PREINITIALIZE` | `DoPreInitialize`, `XTimer::ClearTimerCount` |
| 3 | `AD_BLANK` | `DoBlankScreen` |
| 4 | `AD_DRAWFRAME` | `DoDrawFrame` (first call also `CheckMemAvail` + blank) |
| 5 | `AD_CLOSE` | teardown |
| 6 | `AD_BUTTON` | `DoButton(param & 0xFFFF)` |
| 7 | `AD_KEYDOWN` | `UserInput::KeyModifiers`, `UserInput::SetKeyDown` |
| 8 | `AD_KEYUP` | `UserInput::SetKeyDown(key, 0)` |
| 9 | `AD_PAINT` | `DoPaint` |
| 21 | `AD_LBUTTONDOWN` | `AfterDarkModule::DoLButtonDown` |
| 24 | `AD_LBUTTONHELD` | `AfterDarkModule::DoLButtonHeld` |
| 27 | `AD_LBUTTONUP` | `AfterDarkModule::DoLButtonUp` |
| 30 | `AD_MOUSEMOVE` | `AfterDarkModule::DoMouseMove` |

Names in the left column are ours; the right column is what the binaries say.
Unlisted values in 0–30 fall through to the default case.

Note the renumbering versus AD3, where `1` was BLANK and `3` was CLOSE. **A host
written to the published AD3 constants would send the wrong messages.**

---

## 4. Lifecycle

Reconstructed from the host's load path (`0x4022d1`), its
preinitialise/blank helper (`0x4020b5`), its main loop (`0x401892`) and its
teardown (`0x402241`):

```
  LoadLibraryA("FOO.AD")
  GetProcAddress "_Module@4"  ->  fall back to  "Module"

  Module(AD_MODULESELECTED)        # 0   host sets flag "selected"
    │
    ├── on going to sleep:
    │     Module(AD_PREINITIALIZE) # 2
    │     Module(AD_BLANK)         # 3   host sets flag "initialised"
    │
    ├── main loop, every iteration:
    │     Module(AD_DRAWFRAME)     # 4
    │     if (repaint_pending)
    │         Module(AD_PAINT)     # 9
    │     ... plus input: 7/8 keys, 21/24/27/30 mouse
    │
    └── on wake:
          Module(AD_CLOSE)         # 5
          Module(AD_MODULEDESELECTED)  # 1
          FreeLibrary
```

The main loop is a plain `GetTickCount`-bounded spin (`0x40186a`: deadline =
now + 2000 ms), sending `AD_DRAWFRAME` as fast as it can go round. **There is no
frame pacing whatsoever** — the empirical confirmation of the pacing problem
already noted in [DESIGN.md](DESIGN.md) §5. A modern host must supply its own
clock.

### Return values

The host checks the return of every call. One value is special:

```
4023ff:  cmp eax,3
402404:  jne ...
402408:  call 0x4020b5            ; re-send PREINITIALIZE then BLANK
```

**A return of 3 means "restart me"** — matching AD3's `RESTART_ME = 3`. The host
responds by replaying messages 2 and 3. Zero means success; the host aborts the
module on other non-zero values.

---

## 5. What a host must do

1. Be a **32-bit x86 process** (unchanged, and unavoidable).
2. `LoadLibrary` the user's `ADXPL510.DLL` before the module — modules import
   150–300 functions from it and will not bind otherwise.
3. `LoadLibrary` the `.AD`; resolve `_Module@4`, else `Module`.
4. Zero a 348-byte `AD_MODULE32`, set `cbSize = 0x15C`, `hWnd`, `hInstance`,
   the client rect, and `iControlValue[0..3]` from the extracted schema.
5. Drive the lifecycle above, **with your own frame clock**, honouring a
   return of 3 as restart.

The C declarations are in [`include/ad_module32.h`](../include/ad_module32.h).

---

## 6. Confidence

Confirmed by direct evidence and cross-checked across two independent modules:
the calling convention, the argument count, the block size and its `cbSize`
field, the control array, the message and parameter offsets, the full message
numbering, the lifecycle order, and the restart return code.

Inferred, and worth verifying at runtime: the exact semantics of `+0x018`
(read first by every module, so a spike will surface any mistake immediately),
the meaning of the `+0x004` mode bits, which rect is which, and the length of
the `+0x050` string buffer.

The remaining risk is no longer "can this be called at all" but "does the
engine need state only `AFTERDAR.SCR` sets up". `+0x018` is where that would
bite, and it is the first thing to test.
