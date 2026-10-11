#include "rexbox/recomp/recomp_runtime.h"

#include "recomp_bridge.h"
#include "rexbox/platform/guarded_call.h"
#include "rexbox/recomp/recomp_abi.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/xbe/xbe_image.h"

#include <algorithm>
#include <cstring>

namespace rexbox::recomp
{

namespace
{

runtime::GuestMemory* g_memory = nullptr;

// The initial thread's stack occupies the top of guest memory.
constexpr uint32_t kInitialStackSize = 1u << 20;

// The FS block (Xbox KPCR, whose first part is the NT_TIB) sits below the
// image base, which the image never uses. The kernel HLE fills it in.
constexpr runtime::GuestAddress kInitialFsBase = 0x00001000;
constexpr uint32_t kFsBlockSize = 0x1000;

void CallGuestFunction(void* context)
{
    reinterpret_cast<GuestFunction>(context)();
}

}  // namespace

void Attach(runtime::GuestMemory& memory, runtime::HleRegistry& registry)
{
    g_memory = &memory;
    AttachBridge(&memory, &registry);
}

bool PrepareBoot(const xbe::XbeImage& image, BootLayout* layout, std::string* error)
{
    const auto fail = [error](const char* message)
    {
        if (error != nullptr)
        {
            *error = message;
        }
        return false;
    };
    if (g_memory == nullptr || g_memory->base() == nullptr)
    {
        return fail("recomp runtime is not attached to guest memory");
    }

    BootLayout result;
    result.code_lo = UINT32_MAX;
    for (const xbe::XbeSection& section : image.sections())
    {
        if (section.IsExecutable())
        {
            result.code_lo = std::min(result.code_lo, section.virtual_address);
            result.code_hi = std::max(result.code_hi, section.virtual_address + section.virtual_size);
        }
    }
    if (result.code_lo >= result.code_hi)
    {
        return fail("image has no executable section");
    }

    const size_t memory_size = g_memory->size();
    if (memory_size < kInitialStackSize * 2 || image.base_address() + image.size_of_image() > memory_size - kInitialStackSize)
    {
        return fail("guest memory too small for the image and the initial stack");
    }
    result.stack_base = static_cast<runtime::GuestAddress>(memory_size - kInitialStackSize);
    // Leave a zero return address on top: if the entry ever returns, its
    // `ret` pops 0 rather than reading past the end of guest memory.
    result.stack_top = static_cast<runtime::GuestAddress>(memory_size - 16);
    result.fs_base = kInitialFsBase;
    std::memset(g_memory->Translate(result.stack_base), 0, kInitialStackSize);
    std::memset(g_memory->Translate(result.fs_base), 0, kFsBlockSize);

    rexbox_recomp_set_memory(reinterpret_cast<ptrdiff_t>(g_memory->base()), result.code_lo, result.code_hi);
    rexbox_recomp_reset_thread(result.stack_top, result.fs_base);
    if (layout != nullptr)
    {
        *layout = result;
    }
    return true;
}

RunReport Run(GuestFunction entry)
{
    const platform::GuardedCallResult guarded =
        platform::RunGuarded(&CallGuestFunction, reinterpret_cast<void*>(entry));

    RunReport report;
    switch (guarded.outcome)
    {
    case platform::GuardedOutcome::kReturned:
        report.reason = StopReason::kReturned;
        break;
    case platform::GuardedOutcome::kStopRequested:
        report.reason = guarded.stop_code == kStopUnresolvedCall ? StopReason::kUnresolvedCall
                                                                 : StopReason::kUnimplementedKernel;
        report.target = TakeStopTarget();
        break;
    case platform::GuardedOutcome::kFault:
        report.reason = StopReason::kFault;
        report.fault_code = guarded.fault_code;
        report.host_access_address = guarded.access_address;
        report.access_is_write = guarded.access_is_write;
        break;
    }

    if (report.reason == StopReason::kFault && g_memory != nullptr)
    {
        const uintptr_t base = reinterpret_cast<uintptr_t>(g_memory->base());
        // A guest address is 32 bits wide, so any access in [base, base + 4 GiB)
        // came from guest code, even past the end of mapped guest memory.
        if (report.host_access_address >= base && report.host_access_address - base <= UINT32_MAX)
        {
            report.access_in_guest_memory = true;
            report.guest_access_address = static_cast<runtime::GuestAddress>(report.host_access_address - base);
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
    return report;
}

}  // namespace rexbox::recomp
