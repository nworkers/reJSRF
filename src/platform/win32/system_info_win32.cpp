// System information contract on Win32.

#include "rexbox/platform/system_info.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

namespace rexbox::platform
{

ProcessMemory QueryProcessMemory()
{
    ProcessMemory result;
    PROCESS_MEMORY_COUNTERS_EX counters = {};
    counters.cb = sizeof(counters);
    // K32GetProcessMemoryInfo (kernel32) with PSAPI_VERSION 2.
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)))
    {
        result.known = true;
        result.working_set = counters.WorkingSetSize;
        result.private_bytes = counters.PrivateUsage;
    }
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
    {
        result.address_space_total = status.ullTotalVirtual;
        result.address_space_used = status.ullTotalVirtual - status.ullAvailVirtual;
    }
    return result;
}

}  // namespace rexbox::platform
