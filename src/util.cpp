#include "util.h"

UINT GetUiDpi()
{
    using GetDpiForSystemFn = UINT(WINAPI*)();
    auto fn = reinterpret_cast<GetDpiForSystemFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem"));
    if (fn) {
        UINT dpi = fn();
        if (dpi)
            return dpi;
    }
    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi > 0 ? (UINT)dpi : 96;
}
