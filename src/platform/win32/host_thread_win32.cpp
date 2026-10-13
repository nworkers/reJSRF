#include "rexbox/platform/host_thread.h"

#include <windows.h>

namespace rexbox::platform
{

namespace
{

struct ThreadStart
{
    void (*fn)(void*);
    void* context;
};

DWORD WINAPI ThreadProc(LPVOID parameter)
{
    ThreadStart start = *static_cast<ThreadStart*>(parameter);
    delete static_cast<ThreadStart*>(parameter);
    start.fn(start.context);
    return 0;
}

}  // namespace

HostThread CreateHostThread(void (*fn)(void*), void* context, size_t stack_size)
{
    auto* start = new ThreadStart{fn, context};
    HANDLE handle = CreateThread(nullptr, stack_size, &ThreadProc, start, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (handle == nullptr)
    {
        delete start;
        return 0;
    }
    return reinterpret_cast<HostThread>(handle);
}

void JoinHostThread(HostThread thread)
{
    if (thread != 0)
    {
        HANDLE handle = reinterpret_cast<HANDLE>(thread);
        WaitForSingleObject(handle, INFINITE);
        CloseHandle(handle);
    }
}

void SleepMilliseconds(uint32_t milliseconds)
{
    Sleep(milliseconds);
}

uint64_t ThreadCpuTimeNanoseconds()
{
    FILETIME creation = {};
    FILETIME exit = {};
    FILETIME kernel = {};
    FILETIME user = {};
    if (GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user) == 0)
    {
        return 0;
    }
    const auto ticks = [](const FILETIME& t) {
        return (static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return (ticks(kernel) + ticks(user)) * 100;  // FILETIME counts 100 ns
}

}  // namespace rexbox::platform
