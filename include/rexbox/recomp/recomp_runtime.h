// Running recompiled guest code: wiring the runtime that generated code
// calls back into, and RecompExecutor, the GuestExecutor that runs guest
// threads on host threads and reports how each one ended.

#ifndef REXBOX_RECOMP_RECOMP_RUNTIME_H
#define REXBOX_RECOMP_RECOMP_RUNTIME_H

#include "rexbox/runtime/guest_cpu.h"
#include "rexbox/runtime/guest_memory.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

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
// Maps a guest VA to its recompiled function (the generated recomp_lookup).
using LookupFunction = GuestFunction (*)(uint32_t guest_va);

// Makes `memory` and `registry` the ones generated code calls back into, and
// sets the memory offset and executable range from the placed `image`.
bool Attach(runtime::GuestMemory& memory, runtime::HleRegistry& registry, const xbe::XbeImage& image,
            std::string* error);

// When true (default false), an indirect call with no target stops the run
// instead of being skipped the way the lifter's fallback does.
void SetStopOnUnresolvedCall(bool stop);

// When true, an overridden XDK function with no HLE export runs its lifted
// original (logged once per function) instead of stopping the run. Defaults
// to the environment variable REXBOX_HLE_FALLBACK=lifted. Diagnostic only.
void SetUnimplementedHleFallback(bool run_lifted);

enum class StopReason
{
    kReturned,               // the start function returned
    kExited,                 // the thread called GuestExecutor::ExitThread
    kUnimplementedKernel,    // called a kernel import with no HLE
    kUnimplementedHle,       // called an overridden XDK function with no HLE export
    kUnresolvedCall,         // an indirect call found no target (when stopping on them)
    kFault,                  // a hardware fault inside guest code
    kNoFunction,             // the start VA has no recompiled function
};

struct ThreadReport
{
    uint32_t thread_id = 0;
    runtime::GuestAddress start = 0;
    StopReason reason = StopReason::kReturned;
    uint32_t exit_code = 0;
    runtime::GuestAddress target = 0;  // kernel thunk VA, XDK function VA or unresolved call target
    const char* hle_library = nullptr; // kUnimplementedHle: the XDK library and symbol
    const char* hle_name = nullptr;
    uint32_t fault_code = 0;
    uintptr_t host_access_address = 0;
    bool access_is_write = false;
    // The faulting access as a guest address (host - base, modulo 2^32, which
    // is how a 32-bit host wraps it), and whether it lies in mapped guest RAM.
    runtime::GuestAddress guest_access_address = 0;
    bool access_in_guest_memory = false;
    runtime::GuestAddress last_icall = 0;
    uint32_t eax = 0, ecx = 0, edx = 0, ebx = 0, esp = 0, ebp = 0, esi = 0, edi = 0;

    // Normal ends: the thread returned or exited.
    bool Ended() const { return reason == StopReason::kReturned || reason == StopReason::kExited; }
};

class RecompExecutor : public runtime::GuestExecutor
{
public:
    explicit RecompExecutor(LookupFunction lookup);

    runtime::GuestCpu& Cpu() override;
    bool StartThread(const runtime::GuestThreadStart& start, std::string* error) override;
    [[noreturn]] void ExitThread(uint32_t exit_code) override;
    uint32_t CallGuest(runtime::GuestAddress function, const uint32_t* arguments, int argument_count,
                       bool* called) override;

    // Waits until every started thread has ended, or until one stopped
    // abnormally. Returns the reports of the threads that finished, the
    // abnormal one last.
    std::vector<ThreadReport> WaitForThreads();
    bool halted() const;

private:
    struct ThreadContext;
    static void ThreadMain(void* context);
    void Finish(const ThreadReport& report);

    LookupFunction lookup_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    int live_threads_ = 0;
    bool halted_ = false;
    std::vector<ThreadReport> finished_;
};

}  // namespace rexbox::recomp

#endif  // REXBOX_RECOMP_RECOMP_RUNTIME_H
