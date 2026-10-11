// Running recompiled guest code: boot preparation, the active runtime that
// generated code calls back into, and a guarded run that reports how the
// guest stopped.

#ifndef REXBOX_RECOMP_RECOMP_RUNTIME_H
#define REXBOX_RECOMP_RECOMP_RUNTIME_H

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>
#include <string>

namespace rexbox::runtime
{
class HleRegistry;
}

namespace rexbox::xbe
{
class XbeImage;
}

namespace rexbox::recomp
{

using GuestFunction = void (*)();

// Where boot preparation put the guest's initial thread.
struct BootLayout
{
    runtime::GuestAddress stack_base = 0;  // lowest address of the stack
    runtime::GuestAddress stack_top = 0;   // initial esp
    runtime::GuestAddress fs_base = 0;     // the thread's FS block (KPCR/TIB)
    runtime::GuestAddress code_lo = 0;
    runtime::GuestAddress code_hi = 0;
};

// Makes `memory` and `registry` the ones generated code calls back into.
// Both must outlive every guest run.
void Attach(runtime::GuestMemory& memory, runtime::HleRegistry& registry);

// Sets the memory offset and code range from `image`, carves the initial
// stack and FS block out of guest memory, and resets the calling thread's
// guest state. Call after Attach and after the image is placed in memory.
bool PrepareBoot(const xbe::XbeImage& image, BootLayout* layout, std::string* error);

enum class StopReason
{
    kReturned,               // the guest function returned
    kUnimplementedKernel,    // called a kernel import with no HLE
    kUnresolvedCall,         // an indirect call found no target (when stopping on them)
    kFault,                  // a hardware fault inside guest code
};

struct RunReport
{
    StopReason reason = StopReason::kReturned;
    runtime::GuestAddress target = 0;  // kernel thunk VA or unresolved call target
    uint32_t fault_code = 0;
    uintptr_t host_access_address = 0;
    bool access_is_write = false;
    bool access_in_guest_memory = false;
    runtime::GuestAddress guest_access_address = 0;  // valid if access_in_guest_memory
    runtime::GuestAddress last_icall = 0;
    uint32_t eax = 0, ecx = 0, edx = 0, ebx = 0, esp = 0, ebp = 0, esi = 0, edi = 0;
};

// Runs `entry` on the calling thread under a guard and reports how it ended.
RunReport Run(GuestFunction entry);

// When true (default false), an indirect call with no target stops the run
// instead of being skipped the way the lifter's fallback does.
void SetStopOnUnresolvedCall(bool stop);

}  // namespace rexbox::recomp

#endif  // REXBOX_RECOMP_RECOMP_RUNTIME_H
