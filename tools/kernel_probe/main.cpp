// rexbox_kernel_probe: exercises kernel HLE exports without a game. A fake
// executor holds the guest registers; each check pushes stdcall arguments on
// a guest stack, calls the export through the registry, and inspects eax and
// guest memory.
//
// Usage: rexbox_kernel_probe        (exit code 0 when every check passes)

#include "rexbox/hle/kernel/kernel_hle.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_cpu.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <string>

namespace
{

using rexbox::runtime::GuestAddress;
using rexbox::runtime::Gpr;

class FakeCpu : public rexbox::runtime::GuestCpu
{
public:
    uint32_t Get(Gpr reg) const override { return regs_[static_cast<int>(reg)]; }
    void Set(Gpr reg, uint32_t value) override { regs_[static_cast<int>(reg)] = value; }
    GuestAddress FsBase() const override { return 0; }

private:
    std::array<uint32_t, 8> regs_{};
};

class FakeExecutor : public rexbox::runtime::GuestExecutor
{
public:
    rexbox::runtime::GuestCpu& Cpu() override { return cpu_; }
    bool StartThread(const rexbox::runtime::GuestThreadStart&, std::string* error) override
    {
        *error = "the probe runs no guest threads";
        return false;
    }
    [[noreturn]] void ExitThread(uint32_t) override { std::abort(); }
    uint32_t CallGuest(GuestAddress, const uint32_t*, int, bool* called) override
    {
        *called = false;
        return 0;
    }

private:
    FakeCpu cpu_;
};

constexpr GuestAddress kStackTop = 0x00080000;
constexpr GuestAddress kScratch = 0x00090000;  // argument blocks
constexpr uint32_t kTimeout = 0x00000102;

rexbox::runtime::GuestMemory* g_memory = nullptr;
rexbox::runtime::HleRegistry* g_registry = nullptr;
FakeExecutor* g_executor = nullptr;
int g_failures = 0;
int g_checks = 0;

// Calls export `name` as stdcall with `args`; returns eax. Fails the check if
// the export did not pop exactly its arguments.
uint32_t Call(const char* name, std::initializer_list<uint32_t> args)
{
    const rexbox::runtime::HleExport* entry = g_registry->FindExport(name);
    if (entry == nullptr)
    {
        std::printf("FAIL %s is not exported\n", name);
        ++g_failures;
        return 0xFFFFFFFF;
    }
    GuestAddress esp = kStackTop;
    const uint32_t* values = args.begin();
    for (size_t i = args.size(); i > 0; --i)
    {
        esp -= 4;
        g_memory->Write32(esp, values[i - 1]);
    }
    esp -= 4;
    g_memory->Write32(esp, 0xDEAD0000);  // return address
    rexbox::runtime::GuestCpu& cpu = g_executor->Cpu();
    cpu.Set(Gpr::kEsp, esp);
    entry->thunk();
    if (cpu.Get(Gpr::kEsp) != kStackTop)
    {
        std::printf("FAIL %s left esp %08X, expected %08X\n", name, cpu.Get(Gpr::kEsp), kStackTop);
        ++g_failures;
    }
    return cpu.Get(Gpr::kEax);
}

void Check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
    }
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
}

// A LARGE_INTEGER relative timeout of `ms` milliseconds at `address`.
GuestAddress Timeout(GuestAddress address, int64_t ms)
{
    const int64_t value = -ms * 10000;
    g_memory->Write32(address, static_cast<uint32_t>(value));
    g_memory->Write32(address + 4, static_cast<uint32_t>(static_cast<uint64_t>(value) >> 32));
    return address;
}

void ProbeEvents()
{
    const GuestAddress handle_out = kScratch;
    const GuestAddress zero = Timeout(kScratch + 0x10, 0);

    Check(Call("NtCreateEvent", {handle_out, 0, 0, 0}) == 0, "NtCreateEvent notification, unsignalled");
    const uint32_t manual = g_memory->Read32(handle_out);
    Check(Call("NtWaitForSingleObject", {manual, 0, zero}) == kTimeout, "unsignalled event times out");
    Call("NtSetEvent", {manual, 0});
    Check(Call("NtWaitForSingleObject", {manual, 0, zero}) == 0, "set event satisfies a wait");
    Check(Call("NtWaitForSingleObject", {manual, 0, zero}) == 0, "notification event stays signalled");
    Call("NtClearEvent", {manual});
    Check(Call("NtWaitForSingleObject", {manual, 0, zero}) == kTimeout, "cleared event times out");

    Call("NtCreateEvent", {handle_out, 0, 1, 1});
    const uint32_t automatic = g_memory->Read32(handle_out);
    Check(Call("NtWaitForSingleObject", {automatic, 0, zero}) == 0, "synchronization event starts signalled");
    Check(Call("NtWaitForSingleObject", {automatic, 0, zero}) == kTimeout, "synchronization event resets on wait");

    const auto started = std::chrono::steady_clock::now();
    Call("NtWaitForSingleObject", {automatic, 0, Timeout(kScratch + 0x10, 30)});
    const auto waited = std::chrono::steady_clock::now() - started;
    Check(waited >= std::chrono::milliseconds(25), "a 30 ms relative timeout waits about 30 ms");

    // Two handles: wait-any picks the signalled one and returns its index.
    Call("NtSetEvent", {manual, 0});
    g_memory->Write32(kScratch + 0x20, automatic);
    g_memory->Write32(kScratch + 0x24, manual);
    Check(Call("NtWaitForMultipleObjectsEx", {2, kScratch + 0x20, 1, 0, 0, zero}) == 1,
          "wait-any returns the index of the signalled object");
    Check(Call("NtWaitForMultipleObjectsEx", {2, kScratch + 0x20, 0, 0, 0, zero}) == kTimeout,
          "wait-all times out while one object is unsignalled");
    Call("NtClose", {manual});
    Check(Call("NtClose", {manual}) == 0xC0000008, "closing a handle twice fails");
}

void ProbeSemaphore()
{
    const GuestAddress handle_out = kScratch;
    const GuestAddress zero = Timeout(kScratch + 0x10, 0);
    Call("NtCreateSemaphore", {handle_out, 0, 2, 2});
    const uint32_t semaphore = g_memory->Read32(handle_out);
    Check(Call("NtWaitForSingleObject", {semaphore, 0, zero}) == 0, "semaphore count 2: first wait");
    Check(Call("NtWaitForSingleObject", {semaphore, 0, zero}) == 0, "semaphore count 1: second wait");
    Check(Call("NtWaitForSingleObject", {semaphore, 0, zero}) == kTimeout, "semaphore count 0 times out");
    Check(Call("NtReleaseSemaphore", {semaphore, 1, kScratch + 0x30}) == 0 && g_memory->Read32(kScratch + 0x30) == 0,
          "release reports the previous count");
    Check(Call("NtReleaseSemaphore", {semaphore, 2, 0}) == 0xC0000047, "release past the limit fails");
}

void ProbeCriticalSection()
{
    const GuestAddress cs = kScratch + 0x100;
    Call("RtlInitializeCriticalSection", {cs});
    Call("RtlEnterCriticalSection", {cs});
    Call("RtlEnterCriticalSection", {cs});
    Check(g_memory->Read32(cs + 0x14) == 2 && g_memory->Read32(cs + 0x10) == 1, "recursive enter counts twice");
    Call("RtlLeaveCriticalSection", {cs});
    Call("RtlLeaveCriticalSection", {cs});
    Check(g_memory->Read32(cs + 0x14) == 0 && g_memory->Read32(cs + 0x10) == 0xFFFFFFFF, "leaving twice frees it");
    Check(Call("RtlTryEnterCriticalSection", {cs}) == 1, "a free section can be try-entered");
}

void ProbeSha1()
{
    const GuestAddress context = kScratch + 0x200;
    const GuestAddress input = kScratch + 0x300;
    const GuestAddress digest = kScratch + 0x340;
    std::memcpy(g_memory->Translate(input), "abc", 3);
    Call("XcSHAInit", {context});
    Call("XcSHAUpdate", {context, input, 3});
    Call("XcSHAFinal", {context, digest});
    static const uint8_t kExpected[20] = {0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
                                          0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D};
    Check(std::memcmp(g_memory->Translate(digest), kExpected, 20) == 0, "SHA-1(\"abc\") (FIPS 180-4 vector)");
}

void ProbeTime()
{
    const GuestAddress fields = kScratch + 0x400;
    const GuestAddress time = kScratch + 0x420;
    // 2001-11-15 12:34:56.789, a Thursday (the Xbox launch).
    const uint16_t in[8] = {2001, 11, 15, 12, 34, 56, 789, 0};
    std::memcpy(g_memory->Translate(fields), in, sizeof(in));
    Check(Call("RtlTimeFieldsToTime", {fields, time}) == 1, "RtlTimeFieldsToTime accepts a valid date");
    std::memset(g_memory->Translate(fields), 0, sizeof(in));
    Call("RtlTimeToTimeFields", {time, fields});
    uint16_t out[8];
    std::memcpy(out, g_memory->Translate(fields), sizeof(out));
    Check(out[0] == 2001 && out[1] == 11 && out[2] == 15 && out[3] == 12 && out[4] == 34 && out[5] == 56 &&
              out[6] == 789 && out[7] == 4,
          "time fields round-trip, weekday Thursday");
}

void ProbeMemory()
{
    const GuestAddress base_io = kScratch + 0x500;
    const GuestAddress size_io = kScratch + 0x504;
    g_memory->Write32(base_io, 0);
    g_memory->Write32(size_io, 0x12345);
    Check(Call("NtAllocateVirtualMemory", {base_io, 0, size_io, 0x3000, 4}) == 0, "NtAllocateVirtualMemory");
    const GuestAddress block = g_memory->Read32(base_io);
    Check(block % 0x10000 == 0 && g_memory->Read32(size_io) == 0x13000, "allocation is 64 KiB aligned, size rounded to pages");
    Check(Call("MmQueryAllocationSize", {block}) == 0x13000, "MmQueryAllocationSize of the block");
    g_memory->Write32(size_io, 0);
    Check(Call("NtFreeVirtualMemory", {base_io, size_io, 0x8000}) == 0, "MEM_RELEASE frees it");
    Check(Call("MmQueryAllocationSize", {block}) == 0, "the block is gone");
}

}  // namespace

int main()
{
    rexbox::runtime::GuestMemory memory;
    if (!memory.Initialize(16u * 1024u * 1024u))
    {
        std::fprintf(stderr, "cannot reserve guest memory\n");
        return 2;
    }
    rexbox::runtime::GuestHeap heap(memory.size());
    heap.Reserve(0, 0x00100000);  // the probe's stack and scratch area
    rexbox::runtime::HleRegistry registry;
    FakeExecutor executor;
    rexbox::runtime::SetEnvironment({&memory, &heap, &registry, &executor});
    g_memory = &memory;
    g_registry = &registry;
    g_executor = &executor;

    rexbox::hle::kernel::RegisterKernelHle(registry);
    const std::filesystem::path scratch = std::filesystem::temp_directory_path() / "rexbox_kernel_probe";
    rexbox::hle::kernel::InitializeKernel({scratch, scratch / "hdd"});

    ProbeEvents();
    ProbeSemaphore();
    ProbeCriticalSection();
    ProbeSha1();
    ProbeTime();
    ProbeMemory();

    std::printf("%d check(s), %d failure(s)\n", g_checks, g_failures);
    std::fflush(stdout);
    // The kernel's tick thread runs until the process ends.
    std::_Exit(g_failures == 0 ? 0 : 1);
}
