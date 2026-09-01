// Application controller: hidden message window, 1-second timer, tray icon,
// boot-time state machine and coordination of the countdown widget / settings dialog.
#pragma once
#include <windows.h>

#include <shellapi.h>

#include "countdown.h"

class App {
public:
    static App* Instance();

    int Run(HINSTANCE inst);

    // ---- interface used by the settings dialog ----
    bool BootFound() const { return bootFound_; }
    int StartMin() const { return startMin_; }
    int EndMin() const { return endMin_; }
    int DurationMin() const { return durationMin_; }
    void FormatBootTime(wchar_t* buf, size_t bufCount) const;
    long long RemainingSeconds() const; // -1 when no boot time; <0 past deadline (overtime)
    void FormatRemaining(wchar_t* buf, size_t bufCount) const; // "hh:mm:ss", negative when overtime
    const wchar_t* Message() const { return message_; } // goodbye alert text
    void ApplySettings(int startMin, int endMin, int durationMin, const wchar_t* message);
    static bool AutorunEnabled();
    static bool SetAutorun(bool enable);

    // Default parameters (also used by the settings dialog's Reset button).
    static constexpr int kDefaultStartMin = 8 * 60 + 30;    // attendance window start 08:30
    static constexpr int kDefaultEndMin = 10 * 60 + 30;     // attendance window end   10:30
    static constexpr int kDefaultDurationMin = 8 * 60;      // countdown duration      08:00
    static constexpr size_t kMaxMessageLen = 128;           // goodbye message, incl. terminator

    // Deletes all settings (HKCU\Software\TTSG and the autorun value) and
    // restores the in-memory state to defaults. Returns false on failure.
    bool ResetAll();

private:
    static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void LoadSettings();
    void ParseCommandLine();
    void Recompute(); // re-scan boot time, refresh countdown state
    void OnTick();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void OpenSettings();
    void ToggleCountdown();
    void SetCountdownShown(bool shown);
    void HideCountdown();

    static App* instance_;

    HINSTANCE inst_ = nullptr;
    HWND hwndMain_ = nullptr;
    HICON icon_ = nullptr;
    NOTIFYICONDATAW nid_{};
    UINT taskbarCreatedMsg_ = 0;

    CountdownWindow countdown_;

    // Parameters (minutes of day)
    int startMin_ = kDefaultStartMin;
    int endMin_ = kDefaultEndMin;
    int durationMin_ = kDefaultDurationMin;

    // Goodbye alert text (persisted as REG_SZ; LoadSettings fills the default).
    wchar_t message_[kMaxMessageLen] = {};

    // Runtime state
    bool bootFound_ = false;
    SYSTEMTIME bootTime_{};
    FILETIME goodbyeFT_{};
    bool notified_ = false;
};
