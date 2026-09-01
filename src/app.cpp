#include "app.h"

#include <shellapi.h>

#include <cstdio>
#include <cstdlib>

#include "alert.h"
#include "bootlog.h"
#include "resource.h"
#include "settingsdlg.h"
#include "strings.h"

namespace {

bool ReadDword(const wchar_t* valueName, DWORD& out)
{
    DWORD value = 0, size = sizeof(value);
    LSTATUS st = RegGetValueW(HKEY_CURRENT_USER, str::kRegAppKey, valueName, RRF_RT_REG_DWORD,
                              nullptr, &value, &size);
    if (st != ERROR_SUCCESS)
        return false;
    out = value;
    return true;
}

void WriteDword(const wchar_t* valueName, DWORD value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, str::kRegAppKey, valueName, REG_DWORD, &value,
                    sizeof(value));
}

bool ReadString(const wchar_t* valueName, wchar_t* buf, size_t bufCount)
{
    DWORD size = (DWORD)(bufCount * sizeof(wchar_t));
    LSTATUS st = RegGetValueW(HKEY_CURRENT_USER, str::kRegAppKey, valueName, RRF_RT_REG_SZ,
                              nullptr, buf, &size);
    if (st != ERROR_SUCCESS)
        return false;
    buf[bufCount - 1] = L'\0'; // RegGetValueW NUL-terminates, but stay safe
    return true;
}

void WriteString(const wchar_t* valueName, const wchar_t* value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, str::kRegAppKey, valueName, REG_SZ, value,
                    (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
}

int ParseInt(const wchar_t* text, int fallback)
{
    if (!text || !*text)
        return fallback;
    wchar_t* end = nullptr;
    long value = wcstol(text, &end, 10);
    if (end == text)
        return fallback;
    return (int)value;
}

int Clamp(int value, int lo, int hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

FILETIME AddMinutes(const FILETIME& ft, int minutes)
{
    ULARGE_INTEGER q;
    q.LowPart = ft.dwLowDateTime;
    q.HighPart = ft.dwHighDateTime;
    q.QuadPart += (ULONGLONG)minutes * 60ULL * 10000000ULL;
    FILETIME result;
    result.dwLowDateTime = q.LowPart;
    result.dwHighDateTime = q.HighPart;
    return result;
}

} // namespace

App* App::instance_ = nullptr;

App* App::Instance()
{
    return instance_;
}

int App::Run(HINSTANCE inst)
{
    instance_ = this;
    inst_ = inst;

    // Marker event used as fallback "work start" timestamp by FindSystemStartupTime.
    bootlog::LogAppStart();

    LoadSettings();
    ParseCommandLine();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = inst;
    wc.lpszClassName = str::kMainClass;
    if (!RegisterClassExW(&wc))
        return 1;

    // Hidden controller window: a plain (non HWND_MESSAGE) top-level so it still
    // receives broadcast messages like WM_DISPLAYCHANGE and TaskbarCreated.
    hwndMain_ = CreateWindowExW(0, str::kMainClass, str::kAppName, WS_OVERLAPPED, 0, 0, 0, 0,
                                nullptr, nullptr, inst, this);
    if (!hwndMain_)
        return 1;

    countdown_.Init(inst, hwndMain_);
    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");
    AddTrayIcon();
    Recompute();
    SetTimer(hwndMain_, IDT_TICK, 1000, nullptr);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    countdown_.SetEnabled(false);
    RemoveTrayIcon();
    if (icon_)
        DestroyIcon(icon_);
    return (int)msg.wParam;
}

void App::LoadSettings()
{
    DWORD value = 0;
    if (ReadDword(str::kRegStart, value))
        startMin_ = Clamp((int)value, 0, 1439);
    if (ReadDword(str::kRegEnd, value))
        endMin_ = Clamp((int)value, 1, 1439);
    if (ReadDword(str::kRegDuration, value))
        durationMin_ = Clamp((int)value, 0, 1439);
    if (startMin_ >= endMin_) {
        startMin_ = kDefaultStartMin;
        endMin_ = kDefaultEndMin;
    }
    if (ReadDword(str::kRegShowCount, value))
        countdown_.SetWantShown(value != 0);

    wcsncpy(message_, str::kMsgDefault, kMaxMessageLen);
    message_[kMaxMessageLen - 1] = L'\0';
    wchar_t msg[kMaxMessageLen];
    if (ReadString(str::kRegMessage, msg, kMaxMessageLen) && msg[0] != L'\0') {
        wcsncpy(message_, msg, kMaxMessageLen);
        message_[kMaxMessageLen - 1] = L'\0';
    }
}

void App::ParseCommandLine()
{
    // Compatibility with the old app's autorun format: "ttsg.exe start end duration"
    // (minutes of day). Also handy for quick testing.
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return;
    if (argc >= 4) {
        startMin_ = Clamp(ParseInt(argv[1], startMin_), 0, 1439);
        endMin_ = Clamp(ParseInt(argv[2], endMin_), 1, 1439);
        durationMin_ = Clamp(ParseInt(argv[3], durationMin_), 0, 1439);
        if (startMin_ >= endMin_)
            endMin_ = Clamp(startMin_ + 1, 1, 1439);
    }
    LocalFree(argv);
}

void App::Recompute()
{
    bootFound_ = bootlog::FindSystemStartupTime(startMin_, endMin_, bootTime_);
    notified_ = false;

    if (bootFound_) {
        FILETIME bootFT{};
        SystemTimeToFileTime(&bootTime_, &bootFT);
        goodbyeFT_ = AddMinutes(bootFT, durationMin_);
        countdown_.SetEnabled(true);
        countdown_.SetRemaining(RemainingSeconds());
        countdown_.EnsureCreated();
    } else {
        goodbyeFT_ = {};
        countdown_.SetEnabled(false);
        countdown_.SetRemaining(CountdownWindow::kNoRemaining);
    }
}

long long App::RemainingSeconds() const
{
    if (!bootFound_)
        return -1;
    SYSTEMTIME now;
    GetLocalTime(&now);
    FILETIME nowFT;
    SystemTimeToFileTime(&now, &nowFT);
    ULARGE_INTEGER g, n;
    g.LowPart = goodbyeFT_.dwLowDateTime;
    g.HighPart = goodbyeFT_.dwHighDateTime;
    n.LowPart = nowFT.dwLowDateTime;
    n.HighPart = nowFT.dwHighDateTime;
    return ((long long)g.QuadPart - (long long)n.QuadPart) / 10000000LL;
}

void App::FormatBootTime(wchar_t* buf, size_t bufCount) const
{
    if (!bootFound_) {
        wcsncpy(buf, str::kNotFound, bufCount);
    } else {
        swprintf(buf, bufCount, L"%02d:%02d:%02d", bootTime_.wHour, bootTime_.wMinute,
                 bootTime_.wSecond);
    }
    buf[bufCount - 1] = L'\0';
}

void App::FormatRemaining(wchar_t* buf, size_t bufCount) const
{
    if (!bootFound_) {
        wcsncpy(buf, str::kNoTime, bufCount);
        buf[bufCount - 1] = L'\0';
        return;
    }
    long long rem = RemainingSeconds();
    if (rem < 0) {
        // Overtime past the deadline: show as negative time.
        rem = -rem;
        swprintf(buf, bufCount, L"-%02lld:%02lld:%02lld", rem / 3600, (rem % 3600) / 60,
                 rem % 60);
    } else {
        swprintf(buf, bufCount, L"%02lld:%02lld:%02lld", rem / 3600, (rem % 3600) / 60,
                 rem % 60);
    }
    buf[bufCount - 1] = L'\0';
}

void App::OnTick()
{
    if (bootFound_) {
        const long long rem = RemainingSeconds();
        countdown_.SetRemaining(rem);
        if (rem <= 0 && !notified_) {
            notified_ = true;
            MessageBeep(MB_ICONINFORMATION);
            ShowGoodbyeAlert(inst_, message_);
        }
    }
    // Self-heal: recreates the window after explorer restart / desktop refresh.
    countdown_.EnsureCreated();
}

void App::AddTrayIcon()
{
    if (!icon_) {
        icon_ = static_cast<HICON>(LoadImageW(inst_, MAKEINTRESOURCEW(IDI_TTSG), IMAGE_ICON, 0,
                                              0, LR_DEFAULTSIZE));
    }
    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwndMain_;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_APP_TRAY;
    nid_.hIcon = icon_;
    wcsncpy(nid_.szTip, str::kAppName, sizeof(nid_.szTip) / sizeof(nid_.szTip[0]) - 1);
    Shell_NotifyIconW(NIM_ADD, &nid_);
}

void App::RemoveTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &nid_);
}

void App::ShowTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;
    AppendMenuW(menu, MF_STRING, IDM_OPEN, str::kTrayOpen);
    UINT toggleFlags = MF_STRING;
    if (!bootFound_)
        toggleFlags |= MF_GRAYED;
    if (bootFound_ && countdown_.WantShown())
        toggleFlags |= MF_CHECKED;
    AppendMenuW(menu, toggleFlags, IDM_TOGGLE_COUNT, str::kTrayCountdown);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, str::kTrayExit);
    SetMenuDefaultItem(menu, IDM_OPEN, FALSE);

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwndMain_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndMain_, nullptr);
    PostMessageW(hwndMain_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void App::OpenSettings()
{
    ShowSettingsDialog(hwndMain_, *this);
}

void App::ToggleCountdown()
{
    if (!bootFound_)
        return;
    SetCountdownShown(!countdown_.WantShown());
}

void App::SetCountdownShown(bool shown)
{
    countdown_.SetWantShown(shown);
    WriteDword(str::kRegShowCount, shown ? 1 : 0);
    if (shown)
        countdown_.EnsureCreated();
}

void App::HideCountdown()
{
    SetCountdownShown(false);
}

void App::ApplySettings(int startMin, int endMin, int durationMin, const wchar_t* message)
{
    startMin_ = Clamp(startMin, 0, 1439);
    endMin_ = Clamp(endMin, 1, 1439);
    durationMin_ = Clamp(durationMin, 0, 1439);
    WriteDword(str::kRegStart, (DWORD)startMin_);
    WriteDword(str::kRegEnd, (DWORD)endMin_);
    WriteDword(str::kRegDuration, (DWORD)durationMin_);

    // Empty text falls back to the default message.
    const wchar_t* msg = (message && message[0] != L'\0') ? message : str::kMsgDefault;
    wcsncpy(message_, msg, kMaxMessageLen);
    message_[kMaxMessageLen - 1] = L'\0';
    WriteString(str::kRegMessage, message_);
    Recompute();
}

bool App::ResetAll()
{
    // Delete the whole settings tree (all values under HKCU\Software\TTSG),
    // then remove the now-empty key itself.
    LSTATUS st = RegDeleteTreeW(HKEY_CURRENT_USER, str::kRegAppKey);
    bool ok = (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND);
    st = RegDeleteKeyW(HKEY_CURRENT_USER, str::kRegAppKey);
    if (st != ERROR_SUCCESS && st != ERROR_FILE_NOT_FOUND)
        ok = false;
    // ...and the autorun value.
    st = RegDeleteKeyValueW(HKEY_CURRENT_USER, str::kRegAutorunKey, str::kAppName);
    if (st != ERROR_SUCCESS && st != ERROR_FILE_NOT_FOUND)
        ok = false;

    startMin_ = kDefaultStartMin;
    endMin_ = kDefaultEndMin;
    durationMin_ = kDefaultDurationMin;
    wcsncpy(message_, str::kMsgDefault, kMaxMessageLen);
    message_[kMaxMessageLen - 1] = L'\0';
    countdown_.SetWantShown(true); // default show/hide intent
    Recompute();
    return ok;
}

bool App::AutorunEnabled()
{
    return RegGetValueW(HKEY_CURRENT_USER, str::kRegAutorunKey, str::kAppName, RRF_RT_REG_SZ,
                        nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

bool App::SetAutorun(bool enable)
{
    if (!enable) {
        LSTATUS st =
            RegDeleteKeyValueW(HKEY_CURRENT_USER, str::kRegAutorunKey, str::kAppName);
        return st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND;
    }
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0)
        return false;
    wchar_t command[MAX_PATH + 8] = {};
    swprintf(command, MAX_PATH + 8, L"\"%s\"", exePath);
    command[MAX_PATH + 8 - 1] = L'\0';
    LSTATUS st = RegSetKeyValueW(HKEY_CURRENT_USER, str::kRegAutorunKey, str::kAppName, REG_SZ,
                                 command, (DWORD)((wcslen(command) + 1) * sizeof(wchar_t)));
    return st == ERROR_SUCCESS;
}

LRESULT CALLBACK App::MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self)
        return self->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == taskbarCreatedMsg_ && taskbarCreatedMsg_ != 0) {
        AddTrayIcon(); // re-register the tray icon after explorer restarts
        return 0;
    }
    switch (msg) {
    case WM_TIMER:
        if (wp == IDT_TICK)
            OnTick();
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        countdown_.Reposition();
        return 0;
    case WM_APP_TRAY:
        switch (lp) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            OpenSettings();
            return 0;
        case WM_RBUTTONUP:
            ShowTrayMenu();
            return 0;
        }
        return 0;
    case WM_APP_COUNT_CLOSED:
        HideCountdown();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_OPEN:
            OpenSettings();
            return 0;
        case IDM_TOGGLE_COUNT:
            ToggleCountdown();
            return 0;
        case IDM_EXIT:
            DestroyWindow(hwndMain_);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
