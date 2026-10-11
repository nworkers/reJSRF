// Xbox kernel (xboxkrnl.exe) export table: ordinal -> name.

#ifndef REXBOX_HLE_KERNEL_KERNEL_EXPORTS_H
#define REXBOX_HLE_KERNEL_KERNEL_EXPORTS_H

#include <cstdint>
#include <span>

namespace rexbox::hle::kernel
{

struct KernelExport
{
    uint32_t ordinal;
    const char* name;
};

// Every named export, sorted by ordinal.
std::span<const KernelExport> KernelExports();

// The export's name, or nullptr for an ordinal the kernel does not export.
const char* KernelExportName(uint32_t ordinal);

}  // namespace rexbox::hle::kernel

#endif  // REXBOX_HLE_KERNEL_KERNEL_EXPORTS_H
