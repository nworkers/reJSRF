#include "rexbox/recomp/recomp_runtime.h"

#include "recomp_bridge.h"
#include "rexbox/platform/guarded_call.h"
#include "rexbox/platform/host_thread.h"
#include "rexbox/recomp/recomp_abi.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/xbe/xbe_image.h"

#include <algorithm>

namespace rexbox::recomp
{

namespace
{

runtime::GuestMemory* g_memory = nullptr;

// Lifted guest calls nest as native C calls and Debug frames are large.
constexpr size_t kGuestThreadHostStack = 64u * 1024u * 1024u;

thread_local uint32_t t_exit_code = 0;

class RecompCpu : public runtime::GuestCpu
{
public:
    uint32_t Get(runtime::Gpr reg) const override { return rexbox_recomp_get_gpr(static_cast<int>(reg)); }
    void Set(runtime::Gpr reg, uint32_t value) override { rexbox_recomp_set_gpr(static_cast<int>(reg), value); }
    runtime::GuestAddress FsBase() const override { return rexbox_recomp_get_fs_base(); }
};

RecompCpu g_cpu;  // stateless: every call reads the calling thread's state

// HLE bindings win over lifted code here as they do for indirect calls.
GuestFunction LookupBinding(uint32_t va)
{
    return RecompLookupManual(va);
}

void CallGuestFunction(void* context)
{
    reinterpret_cast<GuestFunction>(context)();
}

}  // namespace

bool Attach(runtime::GuestMemory& memory, runtime::HleRegistry& registry, const xbe::XbeImage& image,
            std::string* error)
{
    uint32_t code_lo = UINT32_MAX;
    uint32_t code_hi = 0;
    for (const xbe::XbeSection& section : image.sections())
    {
        if (section.IsExecutable())
        {
            code_lo = std::min(code_lo, section.virtual_address);
            code_hi = std::max(code_hi, section.virtual_address + section.virtual_size);
        }
    }
    if (code_lo >= code_hi)
    {
        if (error != nullptr)
        {
            *error = "image has no executable section";
        }
        return false;
    }
    g_memory = &memory;
    AttachBridge(&memory, &registry);
    rexbox_recomp_set_memory(reinterpret_cast<ptrdiff_t>(memory.base()), code_lo, code_hi);
    return true;
}

struct RecompExecutor::ThreadContext
{
    RecompExecutor* executor;
    runtime::GuestThreadStart start;
};

RecompExecutor::RecompExecutor(LookupFunction lookup) : lookup_(lookup)
{
}

runtime::GuestCpu& RecompExecutor::Cpu()
{
    return g_cpu;
}

bool RecompExecutor::StartThread(const runtime::GuestThreadStart& start, std::string* error)
{
    auto* context = new ThreadContext{this, start};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++live_threads_;
    }
    if (platform::CreateHostThread(&ThreadMain, context, kGuestThreadHostStack) == 0)
    {
        delete context;
        std::lock_guard<std::mutex> lock(mutex_);
        --live_threads_;
        if (error != nullptr)
        {
            *error = "cannot create a host thread";
        }
        return false;
    }
    // The host thread is detached in effect: it reports through Finish().
    return true;
}

void RecompExecutor::ExitThread(uint32_t exit_code)
{
    t_exit_code = exit_code;
    platform::RaiseStop(kStopThreadExit);
}

uint32_t RecompExecutor::CallGuest(runtime::GuestAddress function, const uint32_t* arguments, int argument_count,
                                   bool* called)
{
    GuestFunction target = LookupBinding(function);
    if (target == nullptr && lookup_ != nullptr)
    {
        target = lookup_(function);
    }
    if (called != nullptr)
    {
        *called = target != nullptr;
    }
    if (target == nullptr)
    {
        return 0;
    }
    const uint32_t saved_esp = rexbox_recomp_get_gpr(4);
    uint32_t esp = saved_esp;
    for (int i = argument_count - 1; i >= 0; --i)
    {
        esp -= 4;
        g_memory->Write32(esp, arguments[i]);
    }
    esp -= 4;
    g_memory->Write32(esp, 0);  // return address
    rexbox_recomp_set_gpr(4, esp);
    target();
    rexbox_recomp_set_gpr(4, saved_esp);
    return rexbox_recomp_get_gpr(0);
}

void RecompExecutor::ThreadMain(void* raw)
{
    ThreadContext context = *static_cast<ThreadContext*>(raw);
    delete static_cast<ThreadContext*>(raw);
    const runtime::GuestThreadStart& start = context.start;

    if (start.on_start != nullptr)
    {
        start.on_start(start.context);
    }
    rexbox_recomp_reset_thread(start.stack_pointer, start.fs_base);
    ThreadReport report;
    report.thread_id = start.thread_id;
    report.start = start.function;

    GuestFunction function = LookupBinding(start.function);
    if (function == nullptr && context.executor->lookup_ != nullptr)
    {
        function = context.executor->lookup_(start.function);
    }
    if (function == nullptr)
    {
        report.reason = StopReason::kNoFunction;
    }
    else
    {
        const platform::GuardedCallResult guarded =
            platform::RunGuarded(&CallGuestFunction, reinterpret_cast<void*>(function));
        switch (guarded.outcome)
        {
        case platform::GuardedOutcome::kReturned:
            report.reason = StopReason::kReturned;
            report.exit_code = rexbox_recomp_get_gpr(0);
            break;
        case platform::GuardedOutcome::kStopRequested:
            if (guarded.stop_code == kStopThreadExit)
            {
                report.reason = StopReason::kExited;
                report.exit_code = t_exit_code;
            }
            else
            {
                report.reason = guarded.stop_code == kStopUnresolvedCall ? StopReason::kUnresolvedCall
                                                                         : StopReason::kUnimplementedKernel;
                report.target = TakeStopTarget();
            }
            break;
        case platform::GuardedOutcome::kFault:
            report.reason = StopReason::kFault;
            report.fault_code = guarded.fault_code;
            report.host_access_address = guarded.access_address;
            report.access_is_write = guarded.access_is_write;
            if (g_memory != nullptr)
            {
                const uintptr_t base = reinterpret_cast<uintptr_t>(g_memory->base());
                report.guest_access_address = static_cast<runtime::GuestAddress>(guarded.access_address - base);
                report.access_in_guest_memory = guarded.access_address >= base &&
                                                guarded.access_address - base < g_memory->size();
            }
            break;
        }
    }

    RexboxRecompGprs gprs;
    rexbox_recomp_get_gprs(&gprs);
    report.eax = gprs.eax;
    report.ecx = gprs.ecx;
    report.edx = gprs.edx;
    report.ebx = gprs.ebx;
    report.esp = gprs.esp;
    report.ebp = gprs.ebp;
    report.esi = gprs.esi;
    report.edi = gprs.edi;
    report.last_icall = rexbox_recomp_last_icall();

    if (start.on_exit != nullptr)
    {
        start.on_exit(start.context, report.exit_code);
    }
    context.executor->Finish(report);
}

void RecompExecutor::Finish(const ThreadReport& report)
{
    std::lock_guard<std::mutex> lock(mutex_);
    --live_threads_;
    if (!halted_)
    {
        finished_.push_back(report);
        halted_ = !report.Ended();
    }
    changed_.notify_all();
}

std::vector<ThreadReport> RecompExecutor::WaitForThreads()
{
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait(lock, [this] { return live_threads_ == 0 || halted_; });
    return finished_;
}

bool RecompExecutor::halted() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return halted_;
}

}  // namespace rexbox::recomp
