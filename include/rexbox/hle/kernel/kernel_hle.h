// HLE module: Xbox kernel (xboxkrnl.exe ordinal exports).
//
// The kernel is the title's only dynamic import, reached through the XBE
// kernel thunk table. Implementations are exported under the names in
// kernel_exports.h.

#ifndef REXBOX_HLE_KERNEL_KERNEL_HLE_H
#define REXBOX_HLE_KERNEL_KERNEL_HLE_H

#include <cstdint>
#include <vector>

namespace rexbox::runtime
{
class HleRegistry;
}

namespace rexbox::xbe
{
class XbeImage;
}

namespace rexbox::hle::kernel
{

void RegisterKernelHle(runtime::HleRegistry& registry);

// Binds each kernel thunk slot of `image` to the registered export of the same
// ordinal. Returns the ordinals that have no implementation yet.
std::vector<uint32_t> BindKernelImports(const xbe::XbeImage& image, runtime::HleRegistry& registry);

}  // namespace rexbox::hle::kernel

#endif  // REXBOX_HLE_KERNEL_KERNEL_HLE_H
