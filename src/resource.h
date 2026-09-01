// TTSG resource & control IDs
#pragma once

// ---- resources ----
#define IDI_TTSG            101

// ---- tray popup menu ----
#define IDM_OPEN            201
#define IDM_TOGGLE_COUNT    202
#define IDM_EXIT            203

// ---- settings dialog controls ----
#define IDC_START           301
#define IDC_END             302
#define IDC_DURATION        303
#define IDC_AUTORUN         304
#define IDC_BOOT_VALUE      305
#define IDC_REMAIN_VALUE    306
#define IDC_OK              307
#define IDC_CANCEL          308
#define IDC_RESET           309

// ---- goodbye alert controls ----
#define IDC_ALERT_OK        310
#define IDC_MESSAGE         311 // settings dialog: reminder message combo box
#define IDC_BG_ENABLED      312 // settings dialog: countdown background checkbox
#define IDC_BG_COLOR        313 // settings dialog: countdown background color swatch

// ---- timers ----
#define IDT_TICK            1
#define IDT_DLG             2
#define IDT_ALERT           3

// ---- app messages ----
#define WM_APP_TRAY         (WM_APP + 1)
#define WM_APP_COUNT_CLOSED (WM_APP + 2)
