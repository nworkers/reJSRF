// rejsrf: application entry. Orchestration only -- load the user's XBE, place
// it in guest memory, register the HLE modules and apply the title profile.
// Running the recompiled code is not wired yet (no generated code exists).
//
// Usage: rejsrf <path to default.xbe>

#include "rejsrf/jsrf_profile.h"
#include "rexbox/hle/d3d8/d3d8_hle.h"
#include "rexbox/hle/dsound/dsound_hle.h"
#include "rexbox/hle/kernel/kernel_hle.h"
#include "rexbox/hle/xapi/xapi_hle.h"
#include "rexbox/hle/xinput/xinput_hle.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/xbe_loader.h"
#include "rexbox/xbe/xbe_image.h"

#include <cstdio>
#include <string>

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
    const auto missing_kernel = rexbox::hle::kernel::BindKernelImports(*image, registry);
    const size_t title_bindings = rejsrf::BindTitleHle(*image, registry);
    std::printf("hle: %zu exports, kernel imports %zu/%zu bound, %zu title bindings\n", registry.export_count(),
                image->kernel_imports().size() - missing_kernel.size(), image->kernel_imports().size(),
                title_bindings);

    std::printf("recompiled code is not built yet; stopping after load\n");
    return 0;
}
