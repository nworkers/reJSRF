#include "rexbox/platform/virtual_memory.h"

#include <windows.h>

namespace rexbox::platform
{

namespace
{

DWORD ToWin32Protection(PageAccess access)
{
    switch (access)
    {
    case PageAccess::kNone:
        return PAGE_NOACCESS;
    case PageAccess::kRead:
        return PAGE_READONLY;
    case PageAccess::kReadWrite:
        return PAGE_READWRITE;
    case PageAccess::kReadExecute:
        return PAGE_EXECUTE_READ;
    case PageAccess::kReadWriteExecute:
        return PAGE_EXECUTE_READWRITE;
    }
    return PAGE_NOACCESS;
}

}  // namespace

void* AllocateVirtualMemory(size_t size, void* preferred_address, bool require_preferred)
{
    if (preferred_address != nullptr)
    {
        void* placed = VirtualAlloc(preferred_address, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (placed != nullptr || require_preferred)
        {
            return placed;
        }
    }
    return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void FreeVirtualMemory(void* address, size_t /*size*/)
{
    if (address != nullptr)
    {
        VirtualFree(address, 0, MEM_RELEASE);
    }
}

bool ProtectVirtualMemory(void* address, size_t size, PageAccess access)
{
    DWORD previous = 0;
    return VirtualProtect(address, size, ToWin32Protection(access), &previous) != FALSE;
}

size_t PageSize()
{
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    return info.dwPageSize;
}

}  // namespace rexbox::platform
