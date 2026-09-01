// Full-screen goodbye alert: a semi-transparent black overlay covering all
// monitors plus a centered white card with an OK button.
#pragma once
#include <windows.h>

// Shows the alert and blocks until the user dismisses it. The card adapts its
// size to the message text; a null/empty message falls back to the default.
void ShowGoodbyeAlert(HINSTANCE inst, const wchar_t* message);
