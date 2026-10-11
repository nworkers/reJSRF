#include "rexbox/hle/kernel/kernel_hle.h"

#include "rexbox/hle/kernel/kernel_exports.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/xbe/xbe_image.h"

namespace rexbox::hle::kernel
{

void RegisterKernelHle(runtime::HleRegistry& /*registry*/)
{
    // No kernel services are implemented yet; they arrive one subsystem at a
    // time (memory, file I/O, threads and sync, HAL), each in its own file.
}

std::vector<uint32_t> BindKernelImports(const xbe::XbeImage& image, runtime::HleRegistry& registry)
{
    std::vector<uint32_t> missing;
    runtime::GuestAddress slot = image.kernel_thunk_address();
    for (const uint32_t ordinal : image.kernel_imports())
    {
        const char* name = KernelExportName(ordinal);
        if (name == nullptr || !registry.BindGuestAddress(slot, name))
        {
            missing.push_back(ordinal);
        }
        slot += 4;
    }
    return missing;
}

}  // namespace rexbox::hle::kernel
