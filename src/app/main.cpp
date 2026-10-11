// rejsrf: application entry. Orchestration only -- load the user's XBE, place
// it in guest memory, install the runtime environment, register the HLE
// modules, apply the title profile and, when the build carries recompiled
// code, boot the entry point as the first guest thread and report how the
// guest threads ended.
//
// Usage: rejsrf <path to default.xbe>

#include "rejsrf/jsrf_profile.h"
#include "rexbox/hle/d3d8/d3d8_hle.h"
#include "rexbox/hle/dsound/dsound_hle.h"
#include "rexbox/hle/kernel/kernel_exports.h"
#include "rexbox/hle/kernel/kernel_hle.h"
#include "rexbox/hle/xapi/xapi_hle.h"
#include "rexbox/hle/xinput/xinput_hle.h"
#include "rexbox/recomp/recomp_runtime.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/kernel_thunks.h"
#include "rexbox/runtime/xbe_loader.h"
#include "rexbox/xbe/xbe_image.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace
{

void PrintThreadReport(const rexbox::recomp::ThreadReport& report)
{
    using rexbox::recomp::StopReason;
    std::printf("thread %u (start %08X): ", report.thread_id, report.start);
    switch (report.reason)
    {
    case StopReason::kReturned:
        std::printf("returned, eax %08X\n", report.exit_code);
        return;
    case StopReason::kExited:
        std::printf("exited with %08X\n", report.exit_code);
        return;
    case StopReason::kNoFunction:
        std::printf("start address has no recompiled function\n");
        return;
    case StopReason::kUnimplementedKernel:
    {
        const uint32_t ordinal = rexbox::runtime::KernelThunkOrdinal(report.target);
        const char* name = rexbox::hle::kernel::KernelExportName(ordinal);
        std::printf("stopped at unimplemented kernel import %u %s\n", ordinal, name != nullptr ? name : "<unknown>");
        break;
    }
    case StopReason::kUnimplementedHle:
        std::printf("stopped at unimplemented XDK function %s %s (%08X)\n",
                    report.hle_library != nullptr ? report.hle_library : "?",
                    report.hle_name != nullptr ? report.hle_name : "?", report.target);
        break;
    case StopReason::kUnresolvedCall:
        std::printf("stopped at indirect call to %08X with no target\n", report.target);
        break;
    case StopReason::kFault:
        std::printf("fault %08X, %s at guest %08X%s (host %p)\n", report.fault_code,
                    report.access_is_write ? "write" : "read", report.guest_access_address,
                    report.access_in_guest_memory ? "" : ", outside guest RAM",
                    reinterpret_cast<void*>(report.host_access_address));
        break;
    }
    std::printf("     eax %08X ecx %08X edx %08X ebx %08X\n", report.eax, report.ecx, report.edx, report.ebx);
    std::printf("     esp %08X ebp %08X esi %08X edi %08X\n", report.esp, report.ebp, report.esi, report.edi);
    std::printf("     last indirect call %08X\n", report.last_icall);
}

}  // namespace

int main(int argc, char** argv)
{
    std::printf("reJSRF %s\n", REJSRF_VERSION);
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <path to default.xbe>\n", argv[0]);
        return 2;
    }

    std::string error;
    const auto image = rexbox::xbe::XbeImage::Load(argv[1], &error);
    if (!image)
    {
        std::fprintf(stderr, "error: %s: %s\n", argv[1], error.c_str());
        return 1;
    }
    const rejsrf::XbeVariant* variant = rejsrf::IdentifyXbe(*image);
    if (variant == nullptr)
    {
        std::fprintf(stderr, "error: %s is not a supported Jet Set Radio Future build "
                             "(title id %08X, certificate version %08X)\n",
                     argv[1], image->certificate().title_id, image->certificate().version);
        return 1;
    }
    std::printf("xbe: %s\n", variant->label);

    rexbox::runtime::GuestMemory memory;
    if (!memory.Initialize(rexbox::runtime::GuestMemory::kRetailRamSize))
    {
        std::fprintf(stderr, "error: cannot reserve guest memory\n");
        return 1;
    }
    if (!rexbox::runtime::LoadXbeIntoGuestMemory(*image, memory, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    rexbox::runtime::GuestHeap heap(memory.size());
    heap.Reserve(0, image->base_address());  // low memory below the image
    heap.Reserve(image->base_address(), image->size_of_image());
    std::printf("guest memory: %zu MiB, %zu sections placed, %u KiB free\n", memory.size() >> 20,
                image->sections().size(), heap.free_bytes() >> 10);

    const rejsrf::RecompiledLookup lookup = rejsrf::RecompiledLookupFunction();
    rexbox::recomp::RecompExecutor executor(reinterpret_cast<rexbox::recomp::LookupFunction>(lookup));
    rexbox::runtime::HleRegistry registry;
    rexbox::runtime::SetEnvironment({&memory, &heap, &registry, &executor});

    rexbox::hle::kernel::RegisterKernelHle(registry);
    rexbox::hle::xapi::RegisterXapiHle(registry);
    rexbox::hle::d3d8::RegisterD3d8Hle(registry);
    rexbox::hle::dsound::RegisterDsoundHle(registry);
    rexbox::hle::xinput::RegisterXinputHle(registry);
    // The disc is the XBE's folder; the hard disk lives under ./hdd unless
    // REJSRF_HDD_ROOT names another place.
    rexbox::hle::kernel::KernelConfig kernel_config;
    kernel_config.game_directory = std::filesystem::absolute(argv[1]).parent_path();
    const char* hdd_root = std::getenv("REJSRF_HDD_ROOT");
    kernel_config.hdd_root = hdd_root != nullptr ? std::filesystem::path(hdd_root)
                                                 : std::filesystem::current_path() / "hdd";
    rexbox::hle::kernel::InitializeKernel(kernel_config);
    const auto missing_kernel = rexbox::hle::kernel::BindKernelImports(*image, memory, registry);
    const size_t title_bindings = rejsrf::BindTitleHle(*image, registry);
    std::printf("hle: %zu exports, kernel imports %zu/%zu bound, %zu title bindings\n", registry.export_count(),
                image->kernel_imports().size() - missing_kernel.size(), image->kernel_imports().size(),
                title_bindings);

    if (lookup == nullptr)
    {
        std::printf("this build has no recompiled code (run scripts/recomp/regenerate.py); stopping after load\n");
        return 0;
    }
    if (variant != rejsrf::PrimaryVariant())
    {
        std::printf("recompiled code was lifted from the %s build; not running %s\n",
                    rejsrf::PrimaryVariant()->label, variant->label);
        return 0;
    }

    if (!rexbox::recomp::Attach(memory, registry, *image, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    const bool flat_dispatch = rejsrf::InitializeRecompiledDispatch();
    std::printf("boot: entry %08X, %s dispatch\n", image->entry_point(), flat_dispatch ? "flat" : "searched");
    std::fflush(stdout);
    if (!rexbox::hle::kernel::StartInitialThread(*image, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }

    const std::vector<rexbox::recomp::ThreadReport> reports = executor.WaitForThreads();
    for (const rexbox::recomp::ThreadReport& report : reports)
    {
        PrintThreadReport(report);
    }
    std::fflush(stdout);
    // Guest threads may still be blocked; ending the process ends them.
    std::_Exit(executor.halted() ? 3 : 0);
}
