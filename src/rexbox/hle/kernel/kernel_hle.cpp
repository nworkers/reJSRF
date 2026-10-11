#include "rexbox/hle/kernel/kernel_hle.h"

#include "rexbox/hle/kernel/kernel_exports.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/kernel_thunks.h"
#include "rexbox/xbe/xbe_image.h"

namespace rexbox::hle::kernel
{

void RegisterKernelHle(runtime::HleRegistry& /*registry*/)
{
    // No kernel services are implemented yet; they arrive one subsystem at a
    // time (memory, file I/O, threads and sync, HAL), each in its own file.
}

std::vector<uint32_t> BindKernelImports(const xbe::XbeImage& image, runtime::GuestMemory& memory,
                                        runtime::HleRegistry& registry)
{
    std::vector<uint32_t> missing;
    runtime::GuestAddress slot = image.kernel_thunk_address();
    for (const uint32_t ordinal : image.kernel_imports())
    {
        const runtime::GuestAddress thunk = runtime::KernelThunkVa(ordinal);
        if (memory.IsValid(slot, 4))
        {
            uint8_t* bytes = memory.Translate(slot);
            bytes[0] = static_cast<uint8_t>(thunk);
            bytes[1] = static_cast<uint8_t>(thunk >> 8);
            bytes[2] = static_cast<uint8_t>(thunk >> 16);
            bytes[3] = static_cast<uint8_t>(thunk >> 24);
        }
        const char* name = KernelExportName(ordinal);
        if (name == nullptr || !registry.BindGuestAddress(thunk, name))
        {
            missing.push_back(ordinal);
        }
        slot += 4;
    }
    return missing;
}

}  // namespace rexbox::hle::kernel
