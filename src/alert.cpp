#include "alert.h"

#include "resource.h"
#include "strings.h"
#include "util.h"

// The alert is two topmost popup windows: a constant-alpha layered mask that
// covers the whole virtual screen (all monitors), and an opaque white card
// centered on the monitor the user was last working on. Topmost windows need
// no elevation, so everything works without administrator rights.

namespace {

// Logical (96 dpi) layout of the center card.
constexpr int kCardW = 210;
constexpr int kCardH = 110;
constexpr int kMsgFontHeight = 16;
// Same size as a MessageBoxW button, measured on a live system (88x28 @96dpi).
constexpr int kButtonW = 88;
constexpr int kButtonH = 28;
constexpr int kButtonGap = 14; // gap between the card's bottom edge and the button
constexpr BYTE kMaskAlpha = 128; // 50% black overlay

constexpr COLORREF kColorBlack = RGB(0, 0, 0);

RECT VirtualScreenRect()
{
    RECT rc;
    rc.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rc.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rc.right = rc.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rc.bottom = rc.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return rc;
}

// Bounds of the monitor the user was last working on; falls back to the
// primary monitor (MonitorFromWindow also does this for a NULL window).
RECT MonitorRectFor(HWND hwnd)
{
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoW(mon, &mi))
        return mi.rcMonitor;
    RECT rc{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    return rc;
}

} // namespace

class GoodbyeAlert {
public:
    void Show(HINSTANCE inst);

private:
    static LRESULT CALLBACK MaskWndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK CardWndProc(HWND, UINT, WPARAM, LPARAM);
    static GoodbyeAlert* SelfFrom(HWND hwnd, UINT msg, LPARAM lp);
    LRESULT HandleMask(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleCard(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void Relayout();       // re-cover the (possibly changed) virtual screen
    void AssertTopmost();  // keep both windows above everything else
    void Dismiss();

    HINSTANCE inst_ = nullptr;
    HWND mask_ = nullptr;
    HWND card_ = nullptr;
    int buttonTop_ = 0; // card client-y of the OK button (bottom of text area)
    HFONT msgFont_ = nullptr;
    HFONT buttonFont_ = nullptr;
};

void ShowGoodbyeAlert(HINSTANCE inst)
{
    GoodbyeAlert alert;
    alert.Show(inst);
}

GoodbyeAlert* GoodbyeAlert::SelfFrom(HWND hwnd, UINT msg, LPARAM lp)
{
    auto* self = reinterpret_cast<GoodbyeAlert*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<GoodbyeAlert*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self;
}

LRESULT CALLBACK GoodbyeAlert::MaskWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = SelfFrom(hwnd, msg, lp);
    if (self)
        return self->HandleMask(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK GoodbyeAlert::CardWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = SelfFrom(hwnd, msg, lp);
    if (self)
        return self->HandleCard(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void GoodbyeAlert::Show(HINSTANCE inst)
{
    inst_ = inst;
    const UINT dpi = GetUiDpi();
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    wc.lpfnWndProc = MaskWndProc;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = str::kAlertMaskClass;
    RegisterClassExW(&wc); // ignore ERROR_CLASS_ALREADY_EXISTS

    wc.lpfnWndProc = CardWndProc;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    wc.lpszClassName = str::kAlertClass;
    RegisterClassExW(&wc);

    // The mask spans the whole virtual screen, dimming every monitor.
    const RECT vs = VirtualScreenRect();
    mask_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
                            str::kAlertMaskClass, L"", WS_POPUP, vs.left, vs.top,
                            vs.right - vs.left, vs.bottom - vs.top, nullptr, nullptr, inst,
                            this);
    if (!mask_)
        return;
    SetLayeredWindowAttributes(mask_, 0, kMaskAlpha, LWA_ALPHA);

    // The card is centered on the monitor that currently has the foreground;
    // it is owned by the mask so it always stays on top of it.
    const int cardW = S(kCardW);
    const int cardH = S(kCardH);
    const RECT mon = MonitorRectFor(GetForegroundWindow());
    const int x = (mon.left + mon.right) / 2 - cardW / 2;
    const int y = (mon.top + mon.bottom) / 2 - cardH / 2;
    card_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, str::kAlertClass, L"", WS_POPUP,
                            x, y, cardW, cardH, mask_, nullptr, inst, this);
    if (!card_) {
        DestroyWindow(mask_);
        mask_ = nullptr;
        return;
    }

    const int btnW = S(kButtonW);
    const int btnH = S(kButtonH);
    buttonTop_ = cardH - S(kButtonGap) - btnH;
    HWND ok = CreateWindowExW(0, L"BUTTON", str::kOk, WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                              (cardW - btnW) / 2, buttonTop_, btnW, btnH, card_,
                              reinterpret_cast<HMENU>(IDC_ALERT_OK), inst, nullptr);

    // Text font: the system message font, enlarged; button font: normal size.
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    LOGFONTW lf{};
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
        ncm.lfMessageFont.lfCharSet = DEFAULT_CHARSET;
    lf = ncm.lfMessageFont;
    buttonFont_ = CreateFontIndirectW(&lf);
    lf.lfHeight = -S(kMsgFontHeight);
    lf.lfWeight = FW_NORMAL;
    msgFont_ = CreateFontIndirectW(&lf);
    if (ok && buttonFont_)
        SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(buttonFont_), TRUE);

    ShowWindow(mask_, SW_SHOWNOACTIVATE);
    ShowWindow(card_, SW_SHOW);

    // Grab the foreground so the card is seen immediately and Enter works.
    // SetForegroundWindow from a background thread normally fails, so attach
    // to the current foreground thread's input queue while taking over.
    HWND fg = GetForegroundWindow();
    const DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    const DWORD myThread = GetCurrentThreadId();
    const bool attached =
        fgThread && fgThread != myThread && AttachThreadInput(myThread, fgThread, TRUE);
    SetForegroundWindow(card_);
    if (attached)
        AttachThreadInput(myThread, fgThread, FALSE);
    SetFocus(card_);

    // Periodically re-assert topmost status in case another window claims it.
    SetTimer(card_, IDT_ALERT, 1000, nullptr);

    MSG msg{};
    bool quit = false;
    while (IsWindow(card_)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) {
            quit = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    // Do not swallow WM_QUIT: hand it back to the outer message loop.
    if (quit)
        PostQuitMessage((int)msg.wParam);

    if (mask_ && IsWindow(mask_))
        DestroyWindow(mask_); // also destroys the card it owns
    if (msgFont_)
        DeleteObject(msgFont_);
    if (buttonFont_)
        DeleteObject(buttonFont_);
}

void GoodbyeAlert::Relayout()
{
    if (mask_ && IsWindow(mask_)) {
        const RECT vs = VirtualScreenRect();
        SetWindowPos(mask_, HWND_TOPMOST, vs.left, vs.top, vs.right - vs.left,
                     vs.bottom - vs.top, SWP_NOACTIVATE);
    }
    if (card_ && IsWindow(card_)) {
        RECT rc{};
        GetWindowRect(card_, &rc);
        const RECT mon = MonitorRectFor(card_);
        const int x = (mon.left + mon.right) / 2 - (rc.right - rc.left) / 2;
        const int y = (mon.top + mon.bottom) / 2 - (rc.bottom - rc.top) / 2;
        SetWindowPos(card_, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE);
    }
}

void GoodbyeAlert::AssertTopmost()
{
    if (mask_ && IsWindow(mask_))
        SetWindowPos(mask_, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (card_ && IsWindow(card_))
        SetWindowPos(card_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void GoodbyeAlert::Dismiss()
{
    if (card_ && IsWindow(card_))
        DestroyWindow(card_); // ends the modal loop; Show() then frees the mask
}

LRESULT GoodbyeAlert::HandleMask(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_DISPLAYCHANGE:
        Relayout();
        return 0;
    case WM_DESTROY:
        mask_ = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT GoodbyeAlert::HandleCard(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, kColorBlack);
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, msgFont_));
        // Center the text block between the card's top edge and the button's
        // top edge. (DT_VCENTER only works with DT_SINGLELINE, so measure the
        // wrapped text first and position it manually.)
        RECT rcText = rc;
        rcText.bottom = buttonTop_;
        RECT rcCalc = rcText;
        DrawTextW(dc, str::kGoodbyeMsg, -1, &rcCalc, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
        const int textH = rcCalc.bottom - rcCalc.top;
        rcText.top = (buttonTop_ - textH) / 2;
        rcText.bottom = rcText.top + textH;
        DrawTextW(dc, str::kGoodbyeMsg, -1, &rcText, DT_CENTER | DT_WORDBREAK);
        SelectObject(dc, oldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_ALERT_OK) {
            Dismiss();
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (wp == VK_RETURN || wp == VK_ESCAPE || wp == VK_SPACE) {
            Dismiss();
            return 0;
        }
        return 0;
    case WM_LBUTTONDOWN:
        Dismiss();
        return 0;
    case WM_TIMER:
        if (wp == IDT_ALERT) {
            AssertTopmost();
            return 0;
        }
        break;
    case WM_DISPLAYCHANGE:
        Relayout();
        return 0;
    case WM_DESTROY:
        card_ = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
