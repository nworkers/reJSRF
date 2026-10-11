// rejsrf: application entry. Orchestration only -- load the user's XBE, place
// it in guest memory, register the HLE modules, apply the title profile and,
// when the build carries recompiled code, run it from the entry point and
// report where it stopped.
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
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/kernel_thunks.h"
#include "rexbox/runtime/xbe_loader.h"
#include "rexbox/xbe/xbe_image.h"

#include <cstdio>
#include <string>

namespace
{

void PrintRunReport(const rexbox::recomp::RunReport& report)
{
    using rexbox::recomp::StopReason;
    switch (report.reason)
    {
    case StopReason::kReturned:
        std::printf("run: entry point returned\n");
        break;
    case StopReason::kUnimplementedKernel:
    {
        const uint32_t ordinal = rexbox::runtime::KernelThunkOrdinal(report.target);
        const char* name = rexbox::hle::kernel::KernelExportName(ordinal);
        std::printf("run: stopped at unimplemented kernel import %u %s\n", ordinal,
                    name != nullptr ? name : "<unknown>");
        break;
    }
    case StopReason::kUnresolvedCall:
        std::printf("run: stopped at indirect call to %08X with no target\n", report.target);
        break;
    case StopReason::kFault:
        std::printf("run: fault %08X, %s ", report.fault_code, report.access_is_write ? "write" : "read");
        if (report.access_in_guest_memory)
        {
            std::printf("at guest %08X\n", report.guest_access_address);
        }
        else
        {
            std::printf("at host %p\n", reinterpret_cast<void*>(report.host_access_address));
        }
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
    std::printf("guest memory: %zu MiB, %zu sections placed\n", memory.size() >> 20, image->sections().size());

    rexbox::runtime::HleRegistry registry;
    rexbox::hle::kernel::RegisterKernelHle(registry);
    rexbox::hle::xapi::RegisterXapiHle(registry);
    rexbox::hle::d3d8::RegisterD3d8Hle(registry);
    rexbox::hle::dsound::RegisterDsoundHle(registry);
    rexbox::hle::xinput::RegisterXinputHle(registry);
    const auto missing_kernel = rexbox::hle::kernel::BindKernelImports(*image, memory, registry);
    const size_t title_bindings = rejsrf::BindTitleHle(*image, registry);
    std::printf("hle: %zu exports, kernel imports %zu/%zu bound, %zu title bindings\n", registry.export_count(),
                image->kernel_imports().size() - missing_kernel.size(), image->kernel_imports().size(),
                title_bindings);

    const rejsrf::RecompiledFunction entry = rejsrf::RecompiledEntry();
    if (entry == nullptr)
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

    rexbox::recomp::Attach(memory, registry);
    rexbox::recomp::BootLayout layout;
    if (!rexbox::recomp::PrepareBoot(*image, &layout, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    const bool flat_dispatch = rejsrf::InitializeRecompiledDispatch();
    std::printf("boot: code %08X-%08X, stack %08X, fs %08X, %s dispatch\n", layout.code_lo, layout.code_hi,
                layout.stack_top, layout.fs_base, flat_dispatch ? "flat" : "searched");

    PrintRunReport(rexbox::recomp::Run(entry));
    return 0;
}
