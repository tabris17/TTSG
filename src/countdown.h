// Desktop countdown widget: a color-keyed layered child of the desktop host
// (Progman / WorkerW). As a desktop child it is:
//   - fixed in place (borderless child window, cannot be dragged)
//   - below every top-level window (does not cover other windows)
//   - unaffected by Win+D (which only minimizes top-level windows)
// The window is re-created automatically when the desktop host dies
// (explorer restart / desktop refresh) - see App::OnTick.
#pragma once
#include <windows.h>

#include <climits>

class CountdownWindow {
public:
    // Sentinel for SetRemaining(): no boot time was found -> "--:--".
    static constexpr long long kNoRemaining = LLONG_MIN;

    // Default background color (dark gray, keeps the white title readable).
    static constexpr COLORREF kDefaultBgColor = RGB(45, 45, 48);

    void Init(HINSTANCE inst, HWND owner);

    // Whether a boot time was found; when false the widget is destroyed.
    void SetEnabled(bool enabled);
    bool Enabled() const { return enabled_; }

    // User's show/hide intent (persisted by App).
    void SetWantShown(bool want);
    bool WantShown() const { return wantShown_; }

    // Create the window if needed (also recovers after explorer restart).
    void EnsureCreated();
    // Apply show/hide intent to an existing window.
    void ApplyVisibility();
    bool IsShown() const { return hwnd_ && IsWindow(hwnd_) && IsWindowVisible(hwnd_); }

    // Seconds until the deadline. Negative values mean overtime past the
    // deadline and are drawn as negative time: red mm:ss within the first
    // hour, purple hh:mm beyond. kNoRemaining means "no boot time" -> "--:--".
    void SetRemaining(long long seconds);
    // Rounded background plate: disabled = transparent (color-keyed), enabled
    // = filled with `color`. Recreates the window when the color key changes.
    void SetBackground(bool enabled, COLORREF color);
    // Move back to the bottom-right corner of the primary work area.
    void Reposition();

    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void CreateFontIfNeeded();
    void Destroy();
    // Transparent color key; avoids colliding with the background fill color.
    COLORREF EffectiveColorKey() const;

    HINSTANCE inst_ = nullptr;
    HWND owner_ = nullptr;   // controller window, receives WM_APP_COUNT_CLOSED
    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    bool classRegistered_ = false;
    bool enabled_ = false;
    bool wantShown_ = true;
    bool bgEnabled_ = false;
    COLORREF bgColor_ = kDefaultBgColor;
    long long remainingSeconds_ = kNoRemaining;
    int width_ = 0, height_ = 0;
};
