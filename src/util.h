#pragma once
#include <windows.h>

// Effective DPI for UI sizing. Prefers GetDpiForSystem (manifest declares
// PerMonitorV2), falls back to the screen device caps.
UINT GetUiDpi();
