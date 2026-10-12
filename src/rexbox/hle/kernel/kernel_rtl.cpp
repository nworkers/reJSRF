// Runtime library services: critical sections, strings, time conversion,
// status codes, exceptions.

#include "kernel_internal.h"

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;
namespace L = layout;

// RTL_CRITICAL_SECTION: a synchronization-event header (0x10), then
// LockCount (+0x10), RecursionCount (+0x14), OwningThread (+0x18).
constexpr uint32_t kCsLockCount = 0x10;
constexpr uint32_t kCsRecursionCount = 0x14;
constexpr uint32_t kCsOwningThread = 0x18;

GuestAddress CurrentEthread()
{
    KernelThread* thread = CurrentThread();
    return thread != nullptr ? thread->ethread : 0;
}

// VOID RtlInitializeCriticalSection(PRTL_CRITICAL_SECTION CriticalSection)
void RtlInitializeCriticalSection()
{
    HleCall call;
    const GuestAddress cs = call.Arg(0);
    runtime::GuestMemory& memory = Memory();
    memory.Fill(cs, 0, 0x1C);
    memory.Write8(cs + L::kHeaderType, static_cast<uint8_t>(ObjectType::kSynchronizationEvent));
    memory.Write8(cs + L::kHeaderSize, 4);
    memory.Write32(cs + L::kHeaderWaitListHead, cs + L::kHeaderWaitListHead);
    memory.Write32(cs + L::kHeaderWaitListHead + 4, cs + L::kHeaderWaitListHead);
    memory.Write32(cs + kCsLockCount, 0xFFFFFFFF);
    call.Return(0, 1);
}

// VOID RtlEnterCriticalSection(PRTL_CRITICAL_SECTION CriticalSection)
void RtlEnterCriticalSection()
{
    HleCall call;
    const GuestAddress cs = call.Arg(0);
    const GuestAddress self = CurrentEthread();
    runtime::GuestMemory& memory = Memory();
    KernelState& state = State();
    std::unique_lock<std::mutex> lock(state.dispatcher);
    const GuestAddress owner = memory.Read32(cs + kCsOwningThread);
    if (owner == self && memory.Read32(cs + kCsRecursionCount) != 0)
    {
        memory.Write32(cs + kCsRecursionCount, memory.Read32(cs + kCsRecursionCount) + 1);
        memory.Write32(cs + kCsLockCount, memory.Read32(cs + kCsLockCount) + 1);
        lock.unlock();
        call.Return(0, 1);
        return;
    }
    memory.Write32(cs + kCsLockCount, memory.Read32(cs + kCsLockCount) + 1);
    // A wait this long is a deadlock in practice; report the section once so
    // the owner can be identified (diagnostic, no behavior change).
    bool reported = false;
    while (!state.signalled.wait_for(lock, std::chrono::seconds(5),
                                     [&] { return memory.Read32(cs + kCsRecursionCount) == 0; }))
    {
        if (!reported)
        {
            reported = true;
            std::fprintf(stderr,
                         "[kernel] RtlEnterCriticalSection %08X: waiting 5 s; owner %08X self %08X recursion %u "
                         "lock count %d\n",
                         cs, memory.Read32(cs + kCsOwningThread), self, memory.Read32(cs + kCsRecursionCount),
                         static_cast<int32_t>(memory.Read32(cs + kCsLockCount)));
        }
    }
    memory.Write32(cs + kCsOwningThread, self);
    memory.Write32(cs + kCsRecursionCount, 1);
    lock.unlock();
    call.Return(0, 1);
}

// BOOLEAN RtlTryEnterCriticalSection(PRTL_CRITICAL_SECTION CriticalSection)
void RtlTryEnterCriticalSection()
{
    HleCall call;
    const GuestAddress cs = call.Arg(0);
    const GuestAddress self = CurrentEthread();
    runtime::GuestMemory& memory = Memory();
    std::lock_guard<std::mutex> lock(State().dispatcher);
    const uint32_t recursion = memory.Read32(cs + kCsRecursionCount);
    if (recursion != 0 && memory.Read32(cs + kCsOwningThread) != self)
    {
        call.Return(0, 1);
        return;
    }
    memory.Write32(cs + kCsLockCount, memory.Read32(cs + kCsLockCount) + 1);
    memory.Write32(cs + kCsOwningThread, self);
    memory.Write32(cs + kCsRecursionCount, recursion + 1);
    call.Return(1, 1);
}

// VOID RtlLeaveCriticalSection(PRTL_CRITICAL_SECTION CriticalSection)
void RtlLeaveCriticalSection()
{
    HleCall call;
    const GuestAddress cs = call.Arg(0);
    runtime::GuestMemory& memory = Memory();
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        const uint32_t recursion = memory.Read32(cs + kCsRecursionCount);
        memory.Write32(cs + kCsLockCount, memory.Read32(cs + kCsLockCount) - 1);
        if (recursion <= 1)
        {
            memory.Write32(cs + kCsRecursionCount, 0);
            memory.Write32(cs + kCsOwningThread, 0);
            State().signalled.notify_all();
        }
        else
        {
            memory.Write32(cs + kCsRecursionCount, recursion - 1);
        }
    }
    call.Return(0, 1);
}

// VOID RtlInitAnsiString(PANSI_STRING DestinationString, PCSZ SourceString)
void RtlInitAnsiString()
{
    HleCall call;
    const GuestAddress destination = call.Arg(0);
    const GuestAddress source = call.Arg(1);
    runtime::GuestMemory& memory = Memory();
    uint32_t length = 0;
    if (source != 0)
    {
        while (memory.Read8(source + length) != 0 && length < 0xFFFE)
        {
            ++length;
        }
    }
    memory.Write16(destination + 0, static_cast<uint16_t>(length));
    memory.Write16(destination + 2, static_cast<uint16_t>(source != 0 ? length + 1 : 0));
    memory.Write32(destination + 4, source);
    call.Return(0, 2);
}

// BOOLEAN RtlEqualString(PSTRING String1, PSTRING String2, BOOLEAN CaseInsensitive)
void RtlEqualString()
{
    HleCall call;
    runtime::GuestMemory& memory = Memory();
    const GuestAddress a = call.Arg(0);
    const GuestAddress b = call.Arg(1);
    const bool insensitive = (call.Arg(2) & 0xFF) != 0;
    const uint16_t length = memory.Read16(a);
    if (length != memory.Read16(b))
    {
        call.Return(0, 3);
        return;
    }
    const GuestAddress buffer_a = memory.Read32(a + 4);
    const GuestAddress buffer_b = memory.Read32(b + 4);
    for (uint16_t i = 0; i < length; ++i)
    {
        char x = static_cast<char>(memory.Read8(buffer_a + i));
        char y = static_cast<char>(memory.Read8(buffer_b + i));
        if (insensitive)
        {
            x = (x >= 'a' && x <= 'z') ? static_cast<char>(x - 32) : x;
            y = (y >= 'a' && y <= 'z') ? static_cast<char>(y - 32) : y;
        }
        if (x != y)
        {
            call.Return(0, 3);
            return;
        }
    }
    call.Return(1, 3);
}

// SIZE_T RtlCompareMemoryUlong(PVOID Source, SIZE_T Length, ULONG Pattern)
void RtlCompareMemoryUlong()
{
    HleCall call;
    const GuestAddress source = call.Arg(0);
    const uint32_t length = call.Arg(1) & ~3u;
    const uint32_t pattern = call.Arg(2);
    uint32_t matched = 0;
    while (matched < length && Memory().Read32(source + matched) == pattern)
    {
        matched += 4;
    }
    call.Return(matched, 3);
}

// ULONG RtlNtStatusToDosError(NTSTATUS Status)
void RtlNtStatusToDosError()
{
    HleCall call;
    call.Return(NtStatusToDosError(call.Arg(0)), 1);
}

// TIME_FIELDS: Year, Month, Day, Hour, Minute, Second, Milliseconds, Weekday (CSHORT each).
bool IsLeap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysInMonth(int year, int month)
{
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && IsLeap(year) ? 29 : kDays[month - 1];
}

// VOID RtlTimeToTimeFields(PLARGE_INTEGER Time, PTIME_FIELDS TimeFields)
void RtlTimeToTimeFields()
{
    HleCall call;
    runtime::GuestMemory& memory = Memory();
    const GuestAddress time = call.Arg(0);
    const GuestAddress fields = call.Arg(1);
    const uint64_t value = static_cast<uint64_t>(memory.Read32(time)) | (static_cast<uint64_t>(memory.Read32(time + 4)) << 32);
    const uint64_t milliseconds_total = value / 10000;
    uint64_t days = milliseconds_total / 86400000ull;
    const uint64_t in_day = milliseconds_total % 86400000ull;
    const int weekday = static_cast<int>((days + 1) % 7);  // 1601-01-01 was a Monday
    int year = 1601;
    while (days >= static_cast<uint64_t>(IsLeap(year) ? 366 : 365))
    {
        days -= IsLeap(year) ? 366 : 365;
        ++year;
    }
    int month = 1;
    while (days >= static_cast<uint64_t>(DaysInMonth(year, month)))
    {
        days -= DaysInMonth(year, month);
        ++month;
    }
    memory.Write16(fields + 0, static_cast<uint16_t>(year));
    memory.Write16(fields + 2, static_cast<uint16_t>(month));
    memory.Write16(fields + 4, static_cast<uint16_t>(days + 1));
    memory.Write16(fields + 6, static_cast<uint16_t>(in_day / 3600000));
    memory.Write16(fields + 8, static_cast<uint16_t>(in_day / 60000 % 60));
    memory.Write16(fields + 10, static_cast<uint16_t>(in_day / 1000 % 60));
    memory.Write16(fields + 12, static_cast<uint16_t>(in_day % 1000));
    memory.Write16(fields + 14, static_cast<uint16_t>(weekday));
    call.Return(0, 2);
}

// BOOLEAN RtlTimeFieldsToTime(PTIME_FIELDS TimeFields, PLARGE_INTEGER Time)
void RtlTimeFieldsToTime()
{
    HleCall call;
    runtime::GuestMemory& memory = Memory();
    const GuestAddress fields = call.Arg(0);
    const GuestAddress time = call.Arg(1);
    const int year = static_cast<int16_t>(memory.Read16(fields + 0));
    const int month = static_cast<int16_t>(memory.Read16(fields + 2));
    const int day = static_cast<int16_t>(memory.Read16(fields + 4));
    if (year < 1601 || month < 1 || month > 12 || day < 1 || day > DaysInMonth(year, month))
    {
        call.Return(0, 2);
        return;
    }
    uint64_t days = 0;
    for (int y = 1601; y < year; ++y)
    {
        days += IsLeap(y) ? 366 : 365;
    }
    for (int m = 1; m < month; ++m)
    {
        days += static_cast<uint64_t>(DaysInMonth(year, m));
    }
    days += static_cast<uint64_t>(day - 1);
    const uint64_t milliseconds = days * 86400000ull + memory.Read16(fields + 6) * 3600000ull +
                                  memory.Read16(fields + 8) * 60000ull + memory.Read16(fields + 10) * 1000ull +
                                  memory.Read16(fields + 12);
    const uint64_t value = milliseconds * 10000;
    memory.Write32(time, static_cast<uint32_t>(value));
    memory.Write32(time + 4, static_cast<uint32_t>(value >> 32));
    call.Return(1, 2);
}

}  // namespace

uint32_t NtStatusToDosError(uint32_t status)
{
    uint32_t error = 317;  // ERROR_MR_MID_NOT_FOUND
    switch (status)
    {
    case kStatusSuccess: error = 0; break;
    case kStatusTimeout: error = 1460; break;          // ERROR_TIMEOUT
    case kStatusPending: error = 997; break;           // ERROR_IO_PENDING
    case kStatusNoMoreFiles: error = 18; break;        // ERROR_NO_MORE_FILES
    case kStatusUnsuccessful: error = 31; break;       // ERROR_GEN_FAILURE
    case kStatusNotImplemented: error = 1; break;      // ERROR_INVALID_FUNCTION
    case kStatusInvalidHandle: error = 6; break;       // ERROR_INVALID_HANDLE
    case kStatusInvalidParameter: error = 87; break;   // ERROR_INVALID_PARAMETER
    case kStatusNoSuchFile: error = 2; break;          // ERROR_FILE_NOT_FOUND
    case kStatusEndOfFile: error = 38; break;          // ERROR_HANDLE_EOF
    case kStatusNoMemory: error = 8; break;            // ERROR_NOT_ENOUGH_MEMORY
    case kStatusAccessDenied: error = 5; break;        // ERROR_ACCESS_DENIED
    case kStatusObjectNameNotFound: error = 2; break;  // ERROR_FILE_NOT_FOUND
    case kStatusObjectNameCollision: error = 183; break;  // ERROR_ALREADY_EXISTS
    case kStatusObjectPathNotFound: error = 3; break;  // ERROR_PATH_NOT_FOUND
    default: break;
    }
    return error;
}

void RegisterRtlExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "RtlInitializeCriticalSection", &RtlInitializeCriticalSection);
    registry.AddExport("kernel", "RtlEnterCriticalSection", &RtlEnterCriticalSection);
    registry.AddExport("kernel", "RtlTryEnterCriticalSection", &RtlTryEnterCriticalSection);
    registry.AddExport("kernel", "RtlLeaveCriticalSection", &RtlLeaveCriticalSection);
    registry.AddExport("kernel", "RtlInitAnsiString", &RtlInitAnsiString);
    registry.AddExport("kernel", "RtlEqualString", &RtlEqualString);
    registry.AddExport("kernel", "RtlCompareMemoryUlong", &RtlCompareMemoryUlong);
    registry.AddExport("kernel", "RtlNtStatusToDosError", &RtlNtStatusToDosError);
    registry.AddExport("kernel", "RtlTimeToTimeFields", &RtlTimeToTimeFields);
    registry.AddExport("kernel", "RtlTimeFieldsToTime", &RtlTimeFieldsToTime);
}

}  // namespace rexbox::hle::kernel
