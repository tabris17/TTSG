// Boot time detection via Windows event logs.
// Ported from old_src/src/main.pas: FindSystemStartupTime / EventLog marker.
#pragma once
#include <windows.h>

namespace bootlog {

// Event ID 12 in the System log is the Kernel-General "operating system started" event.
inline constexpr DWORD kEventIdBoot = 12;
// TTSG writes this marker to the Application log at every startup; it is used as a
// fallback "start of work" timestamp when no boot event is found in the time window.
inline constexpr DWORD kEventIdTtsg = 65535;

// Write the TTSG startup marker event to the Application log.
void LogAppStart();

// Scan the System log (boot event), falling back to the Application log (TTSG marker),
// for the EARLIEST record of today whose local time falls inside
// [startMinute, endMinute] minutes-of-day. Returns true and fills `out` on success.
bool FindSystemStartupTime(int startMinute, int endMinute, SYSTEMTIME& out);

} // namespace bootlog
