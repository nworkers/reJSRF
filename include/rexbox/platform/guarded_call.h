// Platform contract: run a function and catch hardware faults and stop
// requests raised anywhere below it.
//
// Lifted guest code runs as deep native recursion; stopping it (an
// unimplemented service, a fault) has to unwind all of that at once. The
// platform provides the mechanism (Win32: structured exceptions); callers see
// only plain values.

#ifndef REXBOX_PLATFORM_GUARDED_CALL_H
#define REXBOX_PLATFORM_GUARDED_CALL_H

#include <cstdint>

namespace rexbox::platform
{

enum class GuardedOutcome
{
    kReturned,       // the function returned normally
    kStopRequested,  // RaiseStop() was called below it
    kFault,          // a hardware exception (access violation, ...)
};

struct GuardedCallResult
{
    GuardedOutcome outcome = GuardedOutcome::kReturned;
    uint32_t stop_code = 0;          // kStopRequested: the code given to RaiseStop
    uint32_t fault_code = 0;         // kFault: OS exception code
    uintptr_t exception_address = 0; // kFault: host instruction address
    uintptr_t access_address = 0;    // kFault on memory access: host data address
    bool access_is_write = false;
};

// Calls fn(context). Returns how it ended.
GuardedCallResult RunGuarded(void (*fn)(void*), void* context);

// Unwinds to the innermost RunGuarded with kStopRequested and `code`. Must be
// called below a RunGuarded on the same thread.
[[noreturn]] void RaiseStop(uint32_t code);

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_GUARDED_CALL_H
