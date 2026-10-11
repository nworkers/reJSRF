#include "rexbox/hle/kernel/kernel_hle.h"

#include "kernel_internal.h"
#include "kernel_vfs.h"
#include "rexbox/hle/kernel/kernel_exports.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/kernel_thunks.h"
#include "rexbox/xbe/xbe_image.h"

namespace rexbox::hle::kernel
{

void RegisterKernelHle(runtime::HleRegistry& registry)
{
    RegisterDataExports(registry);
    RegisterThreadExports(registry);
    RegisterSyncExports(registry);
    RegisterMemoryExports(registry);
    RegisterObjectExports(registry);
    RegisterRtlExports(registry);
    RegisterHalExports(registry);
    RegisterFileExports(registry);
    RegisterCryptoExports(registry);
}

void InitializeKernel(const KernelConfig& config)
{
    InitializeKernelData();
    VfsInitialize(config.game_directory, config.hdd_root);
}

std::vector<uint32_t> BindKernelImports(const xbe::XbeImage& image, runtime::GuestMemory& memory,
                                        runtime::HleRegistry& registry)
{
    std::vector<uint32_t> missing;
    const auto& data_exports = State().data_exports;
    runtime::GuestAddress slot = image.kernel_thunk_address();
    for (const uint32_t ordinal : image.kernel_imports())
    {
        const char* name = KernelExportName(ordinal);
        const auto data = name != nullptr ? data_exports.find(name) : data_exports.end();
        if (data != data_exports.end())
        {
            memory.Write32(slot, data->second);
        }
        else
        {
            const runtime::GuestAddress thunk = runtime::KernelThunkVa(ordinal);
            memory.Write32(slot, thunk);
            if (name == nullptr || !registry.BindGuestAddress(thunk, name))
            {
                missing.push_back(ordinal);
            }
        }
        slot += 4;
    }
    return missing;
}

}  // namespace rexbox::hle::kernel
