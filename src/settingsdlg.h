// Settings dialog (built in code, no .rc template - avoids rc/windres codepage
// issues with Chinese strings). Shows the detected boot time and a live countdown.
#pragma once
#include <windows.h>

class App;

// Modal settings dialog. Singleton: if one is already open it is activated instead.
void ShowSettingsDialog(HWND owner, App& app);
