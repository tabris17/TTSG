#include "countdown.h"

#include <cstdio>
#include <cstdlib>

#include "resource.h"
#include "strings.h"
#include "util.h"

namespace {

// Logical (96 dpi) geometry, mirroring old_src/src/countdown.lfm
constexpr int kLogicalWidth = 320;
constexpr int kLogicalHeight = 70;
constexpr int kLogicalFontHeight = 50;
constexpr int kMargin = 10;
constexpr int kTimeLeft = 150;

constexpr COLORREF kColorWhite = RGB(255, 255, 255);
constexpr COLORREF kColorYellow = RGB(255, 215, 0);
constexpr COLORREF kColorRed = RGB(255, 0, 0);
constexpr COLORREF kColorPurple = RGB(191, 0, 255);
constexpr COLORREF kColorKey = RGB(0, 0, 0);

// mm:ss is used instead of hh:mm for the last hour before the deadline and
// the first hour of overtime.
constexpr long long kLastHourSeconds = 3600;

// Locate the desktop host window our widget should live in.
// Classic layout: Progman directly contains SHELLDLL_DefView.
// Modern layout (Win10/11 with wallpaper host): after nudging Progman with
// message 0x052C a WorkerW window takes over; the wallpaper is drawn by the
// WorkerW created right after the one hosting SHELLDLL_DefView.
HWND FindDesktopHost()
{
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (progman && FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr))
        return progman;

    if (progman) {
        DWORD_PTR result = 0;
        SendMessageTimeoutW(progman, 0x052C, 0x000D, 1, SMTO_NORMAL, 1000, &result);
    }

    HWND defViewHost = nullptr;
    EnumWindows(
        [](HWND top, LPARAM param) -> BOOL {
            if (FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr)) {
                *reinterpret_cast<HWND*>(param) = top;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&defViewHost));

    if (defViewHost) {
        HWND wallpaperHost = FindWindowExW(nullptr, defViewHost, L"WorkerW", nullptr);
        return wallpaperHost ? wallpaperHost : defViewHost;
    }
    return progman; // last resort; may be nullptr while explorer is restarting
}

// Work area and full bounds of the primary monitor, from a single GetMonitorInfo
// call so the two rectangles are always consistent (SPI_GETWORKAREA must not be
// mixed in: it reports the monitor hosting the taskbar, which can differ).
bool GetPrimaryMonitorRects(RECT& work, RECT& monitor)
{
    struct Context {
        RECT work;
        RECT monitor;
        bool found;
    } ctx{{}, {}, false};
    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR m, HDC, LPRECT, LPARAM param) -> BOOL {
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(m, &mi) && (mi.dwFlags & MONITORINFOF_PRIMARY)) {
                auto* c = reinterpret_cast<Context*>(param);
                c->work = mi.rcWork;
                c->monitor = mi.rcMonitor;
                c->found = true;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ctx));
    if (!ctx.found) {
        ctx.work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        ctx.monitor = ctx.work;
    }
    work = ctx.work;
    monitor = ctx.monitor;
    return true;
}

// Load the embedded font.otf (resource TTSG_FONT) so CreateFont can use family "TTSG".
void LoadEmbeddedFont(HINSTANCE inst)
{
    HRSRC res = FindResourceW(inst, L"TTSG_FONT", RT_RCDATA);
    if (!res)
        return;
    HGLOBAL global = LoadResource(inst, res);
    DWORD size = SizeofResource(inst, res);
    const void* data = global ? LockResource(global) : nullptr;
    if (!data || size == 0)
        return;
    // AddFontMemResourceEx keeps referencing the buffer, so allocate it for the
    // lifetime of the process (same as the old Pascal code did).
    void* copy = malloc(size);
    if (!copy)
        return;
    memcpy(copy, data, size);
    DWORD numFonts = 0;
    AddFontMemResourceEx(copy, size, nullptr, &numFonts);
}

} // namespace

void CountdownWindow::Init(HINSTANCE inst, HWND owner)
{
    inst_ = inst;
    owner_ = owner;
    LoadEmbeddedFont(inst);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = str::kCountdownClass;
    classRegistered_ = RegisterClassExW(&wc) != 0;
}

void CountdownWindow::CreateFontIfNeeded()
{
    if (font_)
        return;
    UINT dpi = GetUiDpi();
    font_ = CreateFontW(-MulDiv(kLogicalFontHeight, dpi, 96), 0, 0, 0, FW_BOLD, FALSE, FALSE,
                        FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, str::kFontFamily);
}

void CountdownWindow::SetEnabled(bool enabled)
{
    enabled_ = enabled;
    if (!enabled)
        Destroy();
}

void CountdownWindow::SetWantShown(bool want)
{
    wantShown_ = want;
    ApplyVisibility();
}

void CountdownWindow::ApplyVisibility()
{
    if (!hwnd_ || !IsWindow(hwnd_))
        return;
    ShowWindow(hwnd_, wantShown_ ? SW_SHOWNA : SW_HIDE);
}

void CountdownWindow::EnsureCreated()
{
    if (!enabled_ || !wantShown_)
        return;
    if (hwnd_ && IsWindow(hwnd_))
        return; // still alive
    hwnd_ = nullptr;

    HWND host = FindDesktopHost();
    if (!host)
        return; // explorer is (re)starting; the next tick retries

    UINT dpi = GetUiDpi();
    width_ = MulDiv(kLogicalWidth, dpi, 96);
    height_ = MulDiv(kLogicalHeight, dpi, 96);

    RECT work{}, monitor{};
    GetPrimaryMonitorRects(work, monitor);
    // Child coordinates are relative to the desktop host, whose origin is the
    // virtual screen's top-left (NOT the primary monitor's when another monitor
    // sits left/above it), while the monitor rects are in virtual-screen coords.
    const int x = work.right - width_ - GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = work.bottom - height_ - GetSystemMetrics(SM_YVIRTUALSCREEN);

    CreateFontIfNeeded();

    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW, str::kCountdownClass, L"",
                            WS_CHILD | WS_CLIPCHILDREN, x, y, width_, height_, host, nullptr,
                            inst_, this);
    if (!hwnd_)
        return;
    SetLayeredWindowAttributes(hwnd_, kColorKey, 0, LWA_COLORKEY);
    // On top of the host's other children (e.g. desktop icons hosted in the same
    // window); still below every top-level window.
    SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ApplyVisibility();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CountdownWindow::SetRemaining(long long seconds)
{
    remainingSeconds_ = seconds;
    if (hwnd_ && IsWindow(hwnd_))
        InvalidateRect(hwnd_, nullptr, FALSE);
}

void CountdownWindow::Reposition()
{
    if (!hwnd_ || !IsWindow(hwnd_) || width_ <= 0)
        return;
    RECT work{}, monitor{};
    GetPrimaryMonitorRects(work, monitor);
    const int x = work.right - width_ - GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = work.bottom - height_ - GetSystemMetrics(SM_YVIRTUALSCREEN);
    SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CountdownWindow::Destroy()
{
    if (hwnd_ && IsWindow(hwnd_))
        DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

LRESULT CALLBACK CountdownWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = reinterpret_cast<CountdownWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<CountdownWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self)
        return self->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CountdownWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd_, &ps);
        RECT rc;
        GetClientRect(hwnd_, &rc);
        FillRect(dc, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(dc, TRANSPARENT);
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font_));

        UINT dpi = GetUiDpi();
        const int margin = MulDiv(kMargin, dpi, 96);

        RECT rcTitle = {margin, margin, 0, 0};
        SetTextColor(dc, kColorWhite);
        DrawTextW(dc, str::kAppName, -1, &rcTitle, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOCLIP);

        wchar_t buf[16];
        COLORREF color = kColorWhite;
        if (remainingSeconds_ == kNoRemaining) {
            wcsncpy(buf, str::kNoTime, 16);
            buf[15] = L'\0';
        } else if (remainingSeconds_ < 0) {
            // Overtime past the deadline: negative time, red -MM:SS within the
            // first hour, purple -HH:MM beyond (so long overtime stands out).
            const long long s = -remainingSeconds_;
            if (s > kLastHourSeconds) {
                color = kColorPurple;
                swprintf(buf, 16, L"-%02lld:%02lld", s / 3600, (s % 3600) / 60);
            } else {
                color = kColorRed;
                swprintf(buf, 16, L"-%02lld:%02lld", s / 60, s % 60);
            }
        } else if (remainingSeconds_ > kLastHourSeconds) {
            const long long s = remainingSeconds_;
            swprintf(buf, 16, L"%02lld:%02lld", s / 3600, (s % 3600) / 60);
        } else {
            swprintf(buf, 16, L"%02lld:%02lld", remainingSeconds_ / 60,
                     remainingSeconds_ % 60);
            color = kColorYellow;
        }

        RECT rcTime = {MulDiv(kTimeLeft, dpi, 96), margin, rc.right - margin, 0};
        SetTextColor(dc, color);
        DrawTextW(dc, buf, -1, &rcTime, DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOCLIP);

        SelectObject(dc, oldFont);
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_RBUTTONUP:
        // Right-click closes the countdown window (re-show via the tray menu).
        PostMessageW(owner_, WM_APP_COUNT_CLOSED, 0, 0);
        return 0;
    case WM_DESTROY:
        hwnd_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
