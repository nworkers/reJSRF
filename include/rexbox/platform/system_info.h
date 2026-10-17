// Platform contract: what the process uses of the host, for diagnostics
// (the settings menu's Info tab).

#ifndef REXBOX_PLATFORM_SYSTEM_INFO_H
#define REXBOX_PLATFORM_SYSTEM_INFO_H

#include <cstdint>

namespace rexbox::platform
{

struct ProcessMemory
{
    bool known = false;
    uint64_t working_set = 0;    // physical memory in use, bytes
    uint64_t private_bytes = 0;  // committed private memory, bytes
    // The process's virtual address space (4 GiB for a large-address-aware
    // 32-bit process on 64-bit Windows): in use and in all.
    uint64_t address_space_used = 0;
    uint64_t address_space_total = 0;
};

ProcessMemory QueryProcessMemory();

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_SYSTEM_INFO_H
