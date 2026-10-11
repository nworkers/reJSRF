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
class GuestMemory;
class HleRegistry;
}

namespace rexbox::xbe
{
class XbeImage;
}

namespace rexbox::hle::kernel
{

void RegisterKernelHle(runtime::HleRegistry& registry);

// Points each kernel thunk slot of the placed image at the ordinal's
// synthetic VA (rexbox/runtime/kernel_thunks.h) and binds that VA to the
// registered export of the same name. Returns the ordinals with no
// implementation yet; their slots are written all the same.
std::vector<uint32_t> BindKernelImports(const xbe::XbeImage& image, runtime::GuestMemory& memory,
                                        runtime::HleRegistry& registry);

}  // namespace rexbox::hle::kernel

#endif  // REXBOX_HLE_KERNEL_KERNEL_HLE_H
