#include "settingsdlg.h"

#include <commctrl.h>
#include <windowsx.h>

#include "app.h"
#include "resource.h"
#include "strings.h"
#include "util.h"

namespace {

// Logical (96 dpi) layout
constexpr int kDlgW = 340;
constexpr int kDlgH = 270;
constexpr int kMargin = 12;
constexpr int kButtonW = 80;
constexpr int kButtonH = 26;
constexpr int kDtpW = 90;
constexpr int kDtpH = 24;

HWND s_openDialog = nullptr;

void SetDtpTime(HWND dtp, int minutes)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    st.wHour = (WORD)(minutes / 60);
    st.wMinute = (WORD)(minutes % 60);
    st.wSecond = 0;
    st.wMilliseconds = 0;
    DateTime_SetSystemtime(dtp, GDT_VALID, &st);
}

int GetDtpMinutes(HWND dtp)
{
    SYSTEMTIME st{};
    if ((int)DateTime_GetSystemtime(dtp, &st) == GDT_ERROR)
        return -1;
    return st.wHour * 60 + st.wMinute;
}

} // namespace

class SettingsDialog {
public:
    INT_PTR Show(HWND owner, App& app);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void CreateControls(UINT dpi);
    void RefreshInfo();
    bool OnOk();
    void OnReset();
    void Finish(INT_PTR result);

    HWND hwnd_ = nullptr;
    HWND owner_ = nullptr;
    App* app_ = nullptr;
    HFONT font_ = nullptr;
    HACCEL accel_ = nullptr;
    INT_PTR result_ = 0;
};

void ShowSettingsDialog(HWND owner, App& app)
{
    if (s_openDialog && IsWindow(s_openDialog)) {
        SetForegroundWindow(s_openDialog);
        return;
    }
    SettingsDialog dlg;
    dlg.Show(owner, app);
}

INT_PTR SettingsDialog::Show(HWND owner, App& app)
{
    owner_ = owner;
    app_ = &app;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    UINT dpi = GetUiDpi();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = str::kSettingsClass;
    wc.hIcon = static_cast<HICON>(
        LoadImageW(inst, MAKEINTRESOURCEW(IDI_TTSG), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;

    // Client rect -> window rect, centered on the primary work area.
    RECT rc = {0, 0, MulDiv(kDlgW, dpi, 96), MulDiv(kDlgH, dpi, 96)};
    AdjustWindowRectEx(&rc, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int winW = rc.right - rc.left;
    const int winH = rc.bottom - rc.top;
    const int x = (work.left + work.right) / 2 - winW / 2;
    const int y = (work.top + work.bottom) / 2 - winH / 2;

    hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME, str::kSettingsClass, str::kSettingsTitle,
                            WS_CAPTION | WS_SYSMENU, x, y, winW, winH, owner, nullptr, inst,
                            this);
    if (!hwnd_)
        return 0;
    s_openDialog = hwnd_;

    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
        font_ = CreateFontIndirectW(&ncm.lfMessageFont);
    SendMessageW(hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);

    CreateControls(dpi);
    SetDtpTime(GetDlgItem(hwnd_, IDC_START), app_->StartMin());
    SetDtpTime(GetDlgItem(hwnd_, IDC_END), app_->EndMin());
    SetDtpTime(GetDlgItem(hwnd_, IDC_DURATION), app_->DurationMin());
    Button_SetCheck(GetDlgItem(hwnd_, IDC_AUTORUN), App::AutorunEnabled() ? BST_CHECKED
                                                                          : BST_UNCHECKED);
    RefreshInfo();

    ACCEL accel[] = {
        {FVIRTKEY, VK_RETURN, IDC_OK},
        {FVIRTKEY, VK_ESCAPE, IDC_CANCEL},
    };
    accel_ = CreateAcceleratorTableW(accel, 2);

    SetTimer(hwnd_, IDT_DLG, 1000, nullptr);
    EnableWindow(owner_, FALSE);
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);

    MSG msg{};
    bool quit = false;
    while (IsWindow(hwnd_)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) {
            quit = true;
            break;
        }
        if (TranslateAcceleratorW(hwnd_, accel_, &msg))
            continue;
        if (!IsDialogMessageW(hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    // Do not swallow WM_QUIT: hand it back to the outer message loop.
    if (quit)
        PostQuitMessage((int)msg.wParam);

    if (accel_)
        DestroyAcceleratorTable(accel_);
    if (font_)
        DeleteObject(font_);
    if (owner_ && IsWindow(owner_)) {
        EnableWindow(owner_, TRUE);
        SetForegroundWindow(owner_);
    }
    return result_;
}

void SettingsDialog::CreateControls(UINT dpi)
{
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    const DWORD child = WS_CHILD | WS_VISIBLE;

    HWND ctl = CreateWindowExW(0, L"STATIC", str::kLabelRange, child | SS_LEFT, S(kMargin),
                               S(14), S(200), S(17), hwnd_, nullptr, nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", str::kLabelFrom, child | SS_LEFT, S(kMargin), S(41), S(20),
                    S(17), hwnd_, nullptr, nullptr, nullptr);
    ctl = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
                          child | WS_TABSTOP | WS_BORDER | DTS_TIMEFORMAT | DTS_UPDOWN, S(40),
                          S(38), S(kDtpW), S(kDtpH), hwnd_,
                          reinterpret_cast<HMENU>(IDC_START), nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", str::kLabelTo, child | SS_LEFT, S(140), S(41), S(20), S(17),
                    hwnd_, nullptr, nullptr, nullptr);
    ctl = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
                          child | WS_TABSTOP | WS_BORDER | DTS_TIMEFORMAT | DTS_UPDOWN, S(165),
                          S(38), S(kDtpW), S(kDtpH), hwnd_, reinterpret_cast<HMENU>(IDC_END),
                          nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", str::kLabelDuration, child | SS_LEFT, S(kMargin), S(79),
                    S(100), S(17), hwnd_, nullptr, nullptr, nullptr);
    ctl = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
                          child | WS_TABSTOP | WS_BORDER | DTS_TIMEFORMAT | DTS_UPDOWN, S(110),
                          S(76), S(kDtpW), S(kDtpH), hwnd_,
                          reinterpret_cast<HMENU>(IDC_DURATION), nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", L"", child | SS_ETCHEDHORZ, S(kMargin), S(112), S(kDlgW - 24),
                    S(8), hwnd_, nullptr, nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", str::kLabelBoot, child | SS_LEFT, S(kMargin), S(128), S(80),
                    S(17), hwnd_, nullptr, nullptr, nullptr);
    ctl = CreateWindowExW(0, L"STATIC", L"", child | SS_LEFT, S(100), S(128), S(200), S(17),
                          hwnd_, reinterpret_cast<HMENU>(IDC_BOOT_VALUE), nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", str::kLabelRemain, child | SS_LEFT, S(kMargin), S(154),
                    S(80), S(17), hwnd_, nullptr, nullptr, nullptr);
    ctl = CreateWindowExW(0, L"STATIC", L"", child | SS_LEFT, S(100), S(154), S(200), S(17),
                          hwnd_, reinterpret_cast<HMENU>(IDC_REMAIN_VALUE), nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", L"", child | SS_ETCHEDHORZ, S(kMargin), S(186), S(kDlgW - 24),
                    S(8), hwnd_, nullptr, nullptr, nullptr);
    ctl = CreateWindowExW(0, L"BUTTON", str::kAutorun, child | WS_TABSTOP | BS_AUTOCHECKBOX,
                          S(kMargin), S(198), S(200), S(20), hwnd_,
                          reinterpret_cast<HMENU>(IDC_AUTORUN), nullptr, nullptr);
    ctl = CreateWindowExW(0, L"BUTTON", str::kReset, child | WS_TABSTOP | BS_PUSHBUTTON,
                          S(kMargin), S(kDlgH - kMargin - kButtonH), S(kButtonW), S(kButtonH),
                          hwnd_, reinterpret_cast<HMENU>(IDC_RESET), nullptr, nullptr);
    ctl = CreateWindowExW(0, L"BUTTON", str::kOk,
                          child | WS_TABSTOP | BS_DEFPUSHBUTTON, S(kDlgW - kMargin - kButtonW),
                          S(kDlgH - kMargin - kButtonH), S(kButtonW), S(kButtonH), hwnd_,
                          reinterpret_cast<HMENU>(IDC_OK), nullptr, nullptr);
    ctl = CreateWindowExW(0, L"BUTTON", str::kCancel, child | WS_TABSTOP | BS_PUSHBUTTON,
                          S(kDlgW - kMargin - kButtonW * 2 - 10),
                          S(kDlgH - kMargin - kButtonH), S(kButtonW), S(kButtonH), hwnd_,
                          reinterpret_cast<HMENU>(IDC_CANCEL), nullptr, nullptr);

    if (font_) {
        for (HWND c = GetWindow(hwnd_, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }

    // Tooltip for the Reset button.
    HWND tooltip = CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP,
                                   CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                   hwnd_, nullptr, nullptr, nullptr);
    if (tooltip) {
        SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, S(260)); // wrap long text
        TOOLINFOW ti{};
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = hwnd_;
        ti.uId = reinterpret_cast<UINT_PTR>(GetDlgItem(hwnd_, IDC_RESET));
        GetClientRect(reinterpret_cast<HWND>(ti.uId), &ti.rect);
        ti.lpszText = const_cast<LPWSTR>(str::kResetTip);
        SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    }
    (void)ctl;
}

void SettingsDialog::RefreshInfo()
{
    wchar_t buf[32];
    app_->FormatBootTime(buf, 32);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_BOOT_VALUE), buf);
    app_->FormatRemaining(buf, 32);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_REMAIN_VALUE), buf);
}

bool SettingsDialog::OnOk()
{
    const int startMin = GetDtpMinutes(GetDlgItem(hwnd_, IDC_START));
    const int endMin = GetDtpMinutes(GetDlgItem(hwnd_, IDC_END));
    const int durationMin = GetDtpMinutes(GetDlgItem(hwnd_, IDC_DURATION));
    if (startMin < 0 || endMin < 0 || durationMin < 0)
        return false;
    if (startMin >= endMin) {
        MessageBoxW(hwnd_, str::kInvalidRange, str::kAppName, MB_OK | MB_ICONWARNING);
        return false;
    }

    const bool wantAutorun =
        Button_GetCheck(GetDlgItem(hwnd_, IDC_AUTORUN)) == BST_CHECKED;
    if (wantAutorun != App::AutorunEnabled()) {
        if (!App::SetAutorun(wantAutorun)) {
            MessageBoxW(hwnd_, str::kRegistryError, str::kAppName, MB_OK | MB_ICONWARNING);
            return false;
        }
    }

    app_->ApplySettings(startMin, endMin, durationMin);
    Finish(1);
    return true;
}

void SettingsDialog::OnReset()
{
    if (MessageBoxW(hwnd_, str::kResetConfirm, str::kAppName,
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;
    if (!app_->ResetAll()) {
        MessageBoxW(hwnd_, str::kRegistryError, str::kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    // Restore the dialog controls to the default values.
    SetDtpTime(GetDlgItem(hwnd_, IDC_START), App::kDefaultStartMin);
    SetDtpTime(GetDlgItem(hwnd_, IDC_END), App::kDefaultEndMin);
    SetDtpTime(GetDlgItem(hwnd_, IDC_DURATION), App::kDefaultDurationMin);
    Button_SetCheck(GetDlgItem(hwnd_, IDC_AUTORUN), App::AutorunEnabled() ? BST_CHECKED
                                                                          : BST_UNCHECKED);
    RefreshInfo();
}

void SettingsDialog::Finish(INT_PTR result)
{
    result_ = result;
    if (hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, IDT_DLG);
        DestroyWindow(hwnd_);
    }
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = reinterpret_cast<SettingsDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<SettingsDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self)
        return self->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT SettingsDialog::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_OK:
            OnOk();
            return 0;
        case IDC_CANCEL:
            Finish(0);
            return 0;
        case IDC_RESET:
            OnReset();
            return 0;
        }
        break;
    case WM_TIMER:
        if (wp == IDT_DLG)
            RefreshInfo();
        return 0;
    case WM_CLOSE:
        Finish(0);
        return 0;
    case WM_DESTROY:
        s_openDialog = nullptr;
        hwnd_ = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
