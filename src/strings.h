// TTSG UI strings (Chinese)
#pragma once

namespace str {

inline constexpr wchar_t kAppName[]        = L"TTSG";
inline constexpr wchar_t kMutexName[]      = L"Local\\TTSG_Instance";
inline constexpr wchar_t kMainClass[]      = L"TTSG_Main";
inline constexpr wchar_t kCountdownClass[] = L"TTSG_Countdown";
inline constexpr wchar_t kSettingsClass[]  = L"TTSG_Settings";
inline constexpr wchar_t kAlertMaskClass[] = L"TTSG_AlertMask";
inline constexpr wchar_t kAlertClass[]     = L"TTSG_Alert";
inline constexpr wchar_t kFontFamily[]     = L"TTSG";

// registry
inline constexpr wchar_t kRegAppKey[]      = L"Software\\TTSG";
inline constexpr wchar_t kRegAutorunKey[]  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t kRegStart[]       = L"StartMinutes";
inline constexpr wchar_t kRegEnd[]         = L"EndMinutes";
inline constexpr wchar_t kRegDuration[]    = L"DurationMinutes";
inline constexpr wchar_t kRegShowCount[]   = L"ShowCountdown";

// tray menu
inline constexpr wchar_t kTrayOpen[]       = L"打开设置…";
inline constexpr wchar_t kTrayCountdown[]  = L"显示倒计时";
inline constexpr wchar_t kTrayExit[]       = L"退出";

// goodbye popup
inline constexpr wchar_t kGoodbyeMsg[]     = L"下班时间到了！";

// settings dialog
inline constexpr wchar_t kSettingsTitle[]  = L"TTSG 设置";
inline constexpr wchar_t kLabelRange[]     = L"考勤时间段：";
inline constexpr wchar_t kLabelFrom[]      = L"从";
inline constexpr wchar_t kLabelTo[]        = L"到";
inline constexpr wchar_t kLabelDuration[]  = L"倒计时时长：";
inline constexpr wchar_t kLabelBoot[]      = L"开机时间：";
inline constexpr wchar_t kLabelRemain[]    = L"剩余时间：";
inline constexpr wchar_t kAutorun[]        = L"开机自动运行";
inline constexpr wchar_t kOk[]             = L"确定";
inline constexpr wchar_t kCancel[]         = L"取消";
inline constexpr wchar_t kReset[]          = L"重置";
inline constexpr wchar_t kResetTip[]       = L"清除所有注册表设置，"
                                             L"并将考勤时间段、倒计时时长、开机自动运行恢复为默认值。";
inline constexpr wchar_t kResetConfirm[]   = L"确定要重置所有设置吗？\n\n"
                                             L"将清除注册表中的全部设置，"
                                             L"并把各项设置恢复为默认值。";
inline constexpr wchar_t kNotFound[]       = L"未找到";
inline constexpr wchar_t kInvalidRange[]   = L"开始时间必须早于结束时间！";
inline constexpr wchar_t kRegistryError[]  = L"写入注册表失败！";
inline constexpr wchar_t kNoTime[]         = L"--:--";

} // namespace str
