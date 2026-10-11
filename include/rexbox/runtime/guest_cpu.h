// Contract between HLE modules and whatever executes guest code.
//
// HLE code never touches the executor's own representation of guest state
// (for recompiled code, the lifter's per-thread globals). It reads and writes
// registers through GuestCpu and starts or ends guest threads through
// GuestExecutor; rexbox_recomp's RecompExecutor implements both.

#ifndef REXBOX_RUNTIME_GUEST_CPU_H
#define REXBOX_RUNTIME_GUEST_CPU_H

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>
#include <string>

namespace rexbox::runtime
{

// General-purpose registers, in x86 encoding order.
enum class Gpr
{
    kEax,
    kEcx,
    kEdx,
    kEbx,
    kEsp,
    kEbp,
    kEsi,
    kEdi,
};

// The calling host thread's guest CPU state.
class GuestCpu
{
public:
    virtual ~GuestCpu() = default;
    virtual uint32_t Get(Gpr reg) const = 0;
    virtual void Set(Gpr reg, uint32_t value) = 0;
    // Guest address of this thread's FS segment (the Xbox KPCR).
    virtual GuestAddress FsBase() const = 0;
};

// How a new guest thread begins: the executor calls `function` as if by a
// `call` whose return address and arguments are already on the stack at
// `stack_pointer`. `on_start` runs first on the new host thread (it may block,
// e.g. while the thread is created suspended). When the guest thread ends, for
// any reason, `on_exit` runs on that host thread with the exit code.
struct GuestThreadStart
{
    GuestAddress function = 0;
    GuestAddress stack_pointer = 0;
    GuestAddress fs_base = 0;
    uint32_t thread_id = 0;
    void (*on_start)(void* context) = nullptr;
    void (*on_exit)(void* context, uint32_t exit_code) = nullptr;
    void* context = nullptr;
};

class GuestExecutor
{
public:
    virtual ~GuestExecutor() = default;

    // Register state of the calling thread.
    virtual GuestCpu& Cpu() = 0;

    // Starts a guest thread on a new host thread.
    virtual bool StartThread(const GuestThreadStart& start, std::string* error) = 0;

    // Ends the calling guest thread with `exit_code`. Unwinds back to the
    // executor; does not return.
    [[noreturn]] virtual void ExitThread(uint32_t exit_code) = 0;

    // Calls a guest function on the calling thread, as `call` would with
    // `arguments` pushed right to left, and returns its eax. esp is restored
    // afterwards whatever the callee's convention. Returns false in `called`
    // when `function` has no executable code.
    virtual uint32_t CallGuest(GuestAddress function, const uint32_t* arguments, int argument_count,
                               bool* called) = 0;
};

}  // namespace rexbox::runtime

#endif  // REXBOX_RUNTIME_GUEST_CPU_H
