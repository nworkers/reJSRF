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

void* AllocateVirtualMemory(size_t size, void* preferred_address, bool require_preferred, bool track_writes)
{
    const DWORD type = MEM_RESERVE | MEM_COMMIT | (track_writes ? MEM_WRITE_WATCH : 0);
    if (preferred_address != nullptr)
    {
        void* placed = VirtualAlloc(preferred_address, size, type, PAGE_READWRITE);
        if (placed != nullptr || require_preferred)
        {
            return placed;
        }
    }
    void* block = VirtualAlloc(nullptr, size, type, PAGE_READWRITE);
    if (block == nullptr && track_writes)
    {
        // Write watching is optional; TakeWrittenPages then reports failure.
        block = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    return block;
}

void FreeVirtualMemory(void* address, size_t /*size*/)
{
    if (address != nullptr)
    {
        VirtualFree(address, 0, MEM_RELEASE);
    }
}

bool TakeWrittenPages(void* address, size_t size, void** pages, size_t capacity, size_t* count)
{
    ULONG_PTR found = capacity;
    DWORD granularity = 0;
    if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, address, size, pages, &found, &granularity) != 0)
    {
        return false;
    }
    // A full array may have left pages out (and unreset): report failure, so
    // the caller assumes them all written.
    if (found >= capacity && size / PageSize() > capacity)
    {
        return false;
    }
    *count = found;
    return true;
}

bool ProtectVirtualMemory(void* address, size_t size, PageAccess access)
{
    DWORD previous = 0;
    return VirtualProtect(address, size, ToWin32Protection(access), &previous) != FALSE;
}

size_t PageSize()
{
    static const size_t page_size = [] {
        SYSTEM_INFO info = {};
        GetSystemInfo(&info);
        return static_cast<size_t>(info.dwPageSize);
    }();
    return page_size;
}

}  // namespace rexbox::platform
