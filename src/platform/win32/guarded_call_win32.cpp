#include "rexbox/platform/guarded_call.h"

#include <windows.h>

namespace rexbox::platform
{

namespace
{

// A user-defined SEH code (bit 29 set, severity error) carrying the stop code
// in its single argument.
constexpr DWORD kStopExceptionCode = 0xE0524558;  // 'REX'

struct FilterState
{
    GuardedCallResult* result;
};

int Filter(EXCEPTION_POINTERS* info, FilterState* state)
{
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    GuardedCallResult& result = *state->result;
    if (record->ExceptionCode == kStopExceptionCode)
    {
        result.outcome = GuardedOutcome::kStopRequested;
        result.stop_code = record->NumberParameters > 0 ? static_cast<uint32_t>(record->ExceptionInformation[0]) : 0;
        return EXCEPTION_EXECUTE_HANDLER;
    }
    result.outcome = GuardedOutcome::kFault;
    result.fault_code = record->ExceptionCode;
    result.exception_address = reinterpret_cast<uintptr_t>(record->ExceptionAddress);
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
    {
        result.access_is_write = record->ExceptionInformation[0] == 1;
        result.access_address = static_cast<uintptr_t>(record->ExceptionInformation[1]);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

GuardedCallResult RunGuarded(void (*fn)(void*), void* context)
{
    GuardedCallResult result;
    FilterState state{&result};
    __try
    {
        fn(context);
        result.outcome = GuardedOutcome::kReturned;
    }
    __except (Filter(GetExceptionInformation(), &state))
    {
    }
    return result;
}

void RaiseStop(uint32_t code)
{
    const ULONG_PTR argument = code;
    RaiseException(kStopExceptionCode, EXCEPTION_NONCONTINUABLE, 1, &argument);
    // RaiseException with a non-continuable code does not return.
    for (;;)
    {
    }
}

}  // namespace rexbox::platform
