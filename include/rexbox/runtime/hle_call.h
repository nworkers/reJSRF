// Argument and result access for an HLE function, which is entered exactly
// where the guest's `call` landed: [esp] holds the return address and stack
// arguments follow it.

#ifndef REXBOX_RUNTIME_HLE_CALL_H
#define REXBOX_RUNTIME_HLE_CALL_H

#include "rexbox/runtime/guest_cpu.h"
#include "rexbox/runtime/guest_memory.h"

#include <cstdint>

namespace rexbox::runtime
{

class HleCall
{
public:
    HleCall();

    // stdcall/cdecl stack argument `index` (0-based).
    uint32_t Arg(int index) const;
    // fastcall register arguments.
    uint32_t Ecx() const { return cpu_.Get(Gpr::kEcx); }
    uint32_t Edx() const { return cpu_.Get(Gpr::kEdx); }

    // Returns `value` in eax and pops the return address plus `stack_args`
    // stack arguments (stdcall; pass 0 for cdecl or register-only fastcall).
    void Return(uint32_t value, int stack_args);
    // 64-bit result in edx:eax.
    void Return64(uint64_t value, int stack_args);

    GuestMemory& memory() const { return memory_; }
    GuestCpu& cpu() const { return cpu_; }

private:
    GuestCpu& cpu_;
    GuestMemory& memory_;
};

}  // namespace rexbox::runtime

#endif  // REXBOX_RUNTIME_HLE_CALL_H
