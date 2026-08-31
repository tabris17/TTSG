#include <windows.h>

#include <commctrl.h>

#include "app.h"
#include "strings.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Single instance only.
    HANDLE mutex = CreateMutexW(nullptr, TRUE, str::kMutexName);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_DATE_CLASSES; // DateTimePicker in the settings dialog
    InitCommonControlsEx(&icc);

    App app;
    return app.Run(hInstance);
}
