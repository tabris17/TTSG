#include "bootlog.h"
#include "strings.h"

#include <cstring>
#include <vector>

namespace {

// 100-ns intervals between 1601-01-01 and 1970-01-01
constexpr ULONGLONG kUnixToFileTimeBias = 116444736000000000ULL;

FILETIME UnixUtcToFileTime(DWORD unixSeconds)
{
    ULARGE_INTEGER q;
    q.QuadPart = kUnixToFileTimeBias + (ULONGLONG)unixSeconds * 10000000ULL;
    FILETIME ft;
    ft.dwLowDateTime = q.LowPart;
    ft.dwHighDateTime = q.HighPart;
    return ft;
}

// Local FILETIME of today's 00:00 plus `minutes`.
bool TodayLocalFileTime(int minutes, FILETIME& out)
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    now.wHour = 0;
    now.wMinute = 0;
    now.wSecond = 0;
    now.wMilliseconds = 0;
    FILETIME ft;
    if (!SystemTimeToFileTime(&now, &ft))
        return false;
    ULARGE_INTEGER q;
    q.LowPart = ft.dwLowDateTime;
    q.HighPart = ft.dwHighDateTime;
    q.QuadPart += (ULONGLONG)minutes * 60ULL * 10000000ULL;
    out.dwLowDateTime = q.LowPart;
    out.dwHighDateTime = q.HighPart;
    return true;
}

bool RecordSourceMatches(const EVENTLOGRECORD& rec, const BYTE* recordBase,
                         const wchar_t* wanted)
{
    // The source name string starts right after the fixed record header. Neither
    // the MS SDK nor MinGW exposes EVENTLOG_UNICODE_TYPE, so instead of testing
    // the record flags we probe both possible encodings of the source name.
    const BYTE* p = recordBase + sizeof(EVENTLOGRECORD);
    const size_t payload = rec.Length > sizeof(EVENTLOGRECORD)
                               ? rec.Length - sizeof(EVENTLOGRECORD)
                               : 0;
    const size_t wantedLen = wcslen(wanted);

    // UTF-16 layout
    if (payload >= (wantedLen + 1) * sizeof(wchar_t)) {
        if (wcsncmp(reinterpret_cast<const wchar_t*>(p), wanted, wantedLen + 1) == 0)
            return true;
    }
    // ANSI layout
    if (payload >= wantedLen + 1) {
        char ansi[64] = {};
        WideCharToMultiByte(CP_ACP, 0, wanted, -1, ansi, sizeof(ansi) - 1, nullptr, nullptr);
        if (strncmp(reinterpret_cast<const char*>(p), ansi, wantedLen + 1) == 0)
            return true;
    }
    return false;
}

struct ScanResult {
    bool found = false;
    FILETIME earliest{};
};

// Walk `logName` backwards (newest first), collecting the earliest record whose local
// time lies inside [ftStart, ftEnd]. Scanning stops once a record older than ftStart
// is seen. If sourceFilter is non-null the record's source name must match as well.
void ScanLog(const wchar_t* logName, DWORD eventId, const wchar_t* sourceFilter,
             const FILETIME& ftStart, const FILETIME& ftEnd, ScanResult& result)
{
    HANDLE h = OpenEventLogW(nullptr, logName);
    if (!h)
        return;

    std::vector<BYTE> buffer(64 * 1024);
    for (;;) {
        DWORD bytesRead = 0, bytesNeeded = 0;
        if (!ReadEventLogW(h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_BACKWARDS_READ, 0,
                           buffer.data(), (DWORD)buffer.size(), &bytesRead, &bytesNeeded)) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && bytesNeeded > buffer.size()) {
                buffer.resize(bytesNeeded);
                continue;
            }
            break; // end of log or unrecoverable error
        }

        DWORD offset = 0;
        while (offset < bytesRead) {
            const auto* rec = reinterpret_cast<const EVENTLOGRECORD*>(buffer.data() + offset);
            if (rec->Length < sizeof(EVENTLOGRECORD))
                break; // corrupt record; bail out of this batch

            FILETIME utc = UnixUtcToFileTime(rec->TimeGenerated);
            FILETIME local{};
            if (!FileTimeToLocalFileTime(&utc, &local)) {
                offset += rec->Length;
                continue;
            }

            if (CompareFileTime(&local, &ftStart) < 0) {
                CloseEventLog(h);
                return; // records only get older from here
            }

            if (rec->EventID == (WORD)eventId) {
                bool match = sourceFilter
                                 ? RecordSourceMatches(*rec, buffer.data() + offset, sourceFilter)
                                 : true;
                if (match && CompareFileTime(&local, &ftEnd) <= 0) {
                    if (!result.found || CompareFileTime(&local, &result.earliest) < 0) {
                        result.earliest = local;
                        result.found = true;
                    }
                }
            }
            offset += rec->Length;
        }
    }
    CloseEventLog(h);
}

} // namespace

namespace bootlog {

void LogAppStart()
{
    HANDLE h = RegisterEventSourceW(nullptr, str::kAppName);
    if (!h)
        return;
    const wchar_t* msg = L"TTSG started";
    ReportEventW(h, EVENTLOG_INFORMATION_TYPE, 0, kEventIdTtsg, nullptr, 1, 0, &msg, nullptr);
    DeregisterEventSource(h);
}

bool FindSystemStartupTime(int startMinute, int endMinute, SYSTEMTIME& out)
{
    if (startMinute < 0 || endMinute <= startMinute)
        return false;

    FILETIME ftStart, ftEnd;
    if (!TodayLocalFileTime(startMinute, ftStart) || !TodayLocalFileTime(endMinute, ftEnd))
        return false;

    ScanResult result;
    // 1st choice: kernel boot event in the System log.
    ScanLog(L"System", kEventIdBoot, nullptr, ftStart, ftEnd, result);
    // Fallback: TTSG's own first startup inside the window (Application log marker).
    if (!result.found)
        ScanLog(L"Application", kEventIdTtsg, str::kAppName, ftStart, ftEnd, result);

    if (!result.found)
        return false;
    return FileTimeToSystemTime(&result.earliest, &out) == TRUE;
}

} // namespace bootlog
