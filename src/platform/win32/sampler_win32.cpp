#include "rexbox/platform/sampler.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <timeapi.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace rexbox::platform
{

namespace
{

// Stack words scanned for a return address into the executable.
constexpr size_t kScanWords = 512;

struct Sampler
{
    HANDLE target = nullptr;
    std::thread thread;
    std::atomic<bool> running{false};
    uintptr_t code_begin = 0;  // the executable's code
    uintptr_t code_end = 0;
    std::map<std::pair<uintptr_t, uintptr_t>, uint64_t> counts;  // (address, caller) -> samples
    uint64_t total = 0;
};

Sampler* g_sampler = nullptr;

void ExecutableCode(uintptr_t* begin, uintptr_t* end)
{
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    *begin = base + nt->OptionalHeader.BaseOfCode;
    *end = *begin + nt->OptionalHeader.SizeOfCode;
}

void Run(Sampler* sampler)
{
    std::vector<uintptr_t> stack(kScanWords);
    while (sampler->running.load())
    {
        // Nothing that allocates may run while the thread is stopped: it may
        // hold the heap lock.
        uintptr_t address = 0;
        size_t words = 0;
        if (SuspendThread(sampler->target) != static_cast<DWORD>(-1))
        {
            CONTEXT context = {};
            context.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(sampler->target, &context) != 0)
            {
#if defined(_M_IX86)
                address = context.Eip;
                const uintptr_t sp = context.Esp;
#else
                address = context.Rip;
                const uintptr_t sp = context.Rsp;
#endif
                // Outside the executable, copy the top of the stack to find
                // the code there that led to this point (the thread's own
                // stack stays mapped while it is stopped).
                MEMORY_BASIC_INFORMATION region = {};
                if ((address < sampler->code_begin || address >= sampler->code_end) &&
                    VirtualQuery(reinterpret_cast<const void*>(sp), &region, sizeof(region)) != 0)
                {
                    const uintptr_t region_end = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
                    words = std::min(kScanWords, (region_end - sp) / sizeof(uintptr_t));
                    std::memcpy(stack.data(), reinterpret_cast<const void*>(sp), words * sizeof(uintptr_t));
                }
            }
            ResumeThread(sampler->target);
        }
        if (address != 0)
        {
            // The first stack word pointing into the executable's code: a
            // return address most of the time (a heuristic; stale values
            // can match too).
            uintptr_t caller = 0;
            for (size_t i = 0; i < words; ++i)
            {
                if (stack[i] >= sampler->code_begin && stack[i] < sampler->code_end)
                {
                    caller = stack[i];
                    break;
                }
            }
            ++sampler->counts[{address, caller}];
            ++sampler->total;
        }
        Sleep(1);
    }
}

// The module holding `address`: its file name and base.
std::string ModuleOf(uintptr_t address, uintptr_t* base)
{
    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(address), &module) == 0)
    {
        *base = 0;
        return "?";
    }
    char path[MAX_PATH] = {};
    GetModuleFileNameA(module, path, MAX_PATH);
    *base = reinterpret_cast<uintptr_t>(module);
    const char* name = path;
    for (const char* p = path; *p != '\0'; ++p)
    {
        if (*p == '\\' || *p == '/')
        {
            name = p + 1;
        }
    }
    return name;
}

}  // namespace

bool StartThreadSampling()
{
    if (g_sampler != nullptr)
    {
        return false;
    }
    HANDLE target = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &target,
                        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0) == 0)
    {
        return false;
    }
    // Sleep(1) sleeps about a millisecond only with a 1 ms timer period.
    timeBeginPeriod(1);
    g_sampler = new Sampler;
    g_sampler->target = target;
    ExecutableCode(&g_sampler->code_begin, &g_sampler->code_end);
    g_sampler->running.store(true);
    g_sampler->thread = std::thread(Run, g_sampler);
    return true;
}

uint64_t StopThreadSampling(const char* path)
{
    Sampler* sampler = g_sampler;
    if (sampler == nullptr)
    {
        return 0;
    }
    g_sampler = nullptr;
    sampler->running.store(false);
    sampler->thread.join();
    timeEndPeriod(1);
    CloseHandle(sampler->target);

    std::vector<std::pair<std::pair<uintptr_t, uintptr_t>, uint64_t>> sorted(sampler->counts.begin(),
                                                                               sampler->counts.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    const uint64_t total = sampler->total;
    const uintptr_t exe_base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    FILE* out = std::fopen(path, "w");
    if (out != nullptr)
    {
        std::fprintf(out, "# samples %llu\n", static_cast<unsigned long long>(total));
        for (const auto& [key, count] : sorted)
        {
            uintptr_t base = 0;
            const std::string module = ModuleOf(key.first, &base);
            std::fprintf(out, "%s %llX %llu", module.c_str(), static_cast<unsigned long long>(key.first - base),
                         static_cast<unsigned long long>(count));
            if (key.second != 0)
            {
                std::fprintf(out, " %llX", static_cast<unsigned long long>(key.second - exe_base));
            }
            std::fprintf(out, "\n");
        }
        std::fclose(out);
    }
    delete sampler;
    return out != nullptr ? total : 0;
}

}  // namespace rexbox::platform
