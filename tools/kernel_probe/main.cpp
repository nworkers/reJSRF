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
#include "rexbox/runtime/kernel_thunks.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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
    // Runs a "guest" function only when the registry binds its address (a
    // kernel thunk or a probe routine), with the arguments on the guest stack.
    uint32_t CallGuest(GuestAddress function, const uint32_t* arguments, int argument_count, bool* called) override
    {
        const rexbox::runtime::HleThunk thunk = rexbox::runtime::Env().registry->Lookup(function);
        *called = thunk != nullptr;
        if (thunk == nullptr)
        {
            return 0;
        }
        rexbox::runtime::GuestMemory& memory = *rexbox::runtime::Env().memory;
        const uint32_t saved_esp = cpu_.Get(Gpr::kEsp);
        uint32_t esp = saved_esp;
        for (int i = argument_count - 1; i >= 0; --i)
        {
            esp -= 4;
            memory.Write32(esp, arguments[i]);
        }
        esp -= 4;
        memory.Write32(esp, 0);
        cpu_.Set(Gpr::kEsp, esp);
        thunk();
        cpu_.Set(Gpr::kEsp, saved_esp);
        return cpu_.Get(Gpr::kEax);
    }

private:
    FakeCpu cpu_;
};

constexpr GuestAddress kStackTop = 0x00080000;
constexpr GuestAddress kScratch = 0x00090000;  // argument blocks
constexpr uint32_t kTimeout = 0x00000102;
constexpr uint32_t kUserApc = 0x000000C0;

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

// XAPI opens "D:\..." with RootDirectory = ObDosDevicesDirectory() (-3);
// \??\D: is the disc, i.e. the game directory.
void ProbeDosDevicesRoot(const std::filesystem::path& game_directory)
{
    std::filesystem::create_directories(game_directory);
    {
        std::ofstream(game_directory / "probe.bin", std::ios::binary) << "probe";
    }
    const GuestAddress text = kScratch + 0x600;
    const GuestAddress name = kScratch + 0x640;
    const GuestAddress attributes = kScratch + 0x650;
    const GuestAddress handle_out = kScratch + 0x660;
    const GuestAddress io_status = kScratch + 0x668;
    const char path[] = "D:\\probe.bin";
    std::memcpy(g_memory->Translate(text), path, sizeof(path));
    g_memory->Write16(name + 0, static_cast<uint16_t>(sizeof(path) - 1));
    g_memory->Write16(name + 2, static_cast<uint16_t>(sizeof(path)));
    g_memory->Write32(name + 4, text);
    g_memory->Write32(attributes + 0, 0xFFFFFFFD);  // ObDosDevicesDirectory()
    g_memory->Write32(attributes + 4, name);
    g_memory->Write32(attributes + 8, 0x40);         // OBJ_CASE_INSENSITIVE
    const uint32_t status = Call("NtOpenFile", {handle_out, 0x80100000, attributes, io_status, 1, 0x60});
    Check(status == 0, "NtOpenFile D:\\probe.bin relative to ObDosDevicesDirectory()");
    if (status == 0)
    {
        Call("NtClose", {g_memory->Read32(handle_out)});
    }
}


// A completion routine the probe binds at kCompletionRoutine:
// VOID (DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped).
constexpr GuestAddress kCompletionRoutine = 0x00097000;
int g_completions = 0;
uint32_t g_completion_args[3] = {};

void ProbeCompletionRoutine()
{
    rexbox::runtime::GuestCpu& cpu = g_executor->Cpu();
    const GuestAddress esp = cpu.Get(Gpr::kEsp);
    for (uint32_t i = 0; i < 3; ++i)
    {
        g_completion_args[i] = g_memory->Read32(esp + 4 + 4 * i);
    }
    ++g_completions;
    cpu.Set(Gpr::kEsp, esp + 4 + 12);
}

// Opens D:\<name> from the game directory; returns the handle or 0.
uint32_t OpenGameFile(const char* name)
{
    const GuestAddress text = kScratch + 0x600;
    const GuestAddress string = kScratch + 0x640;
    const GuestAddress attributes = kScratch + 0x650;
    const GuestAddress handle_out = kScratch + 0x660;
    const GuestAddress io_status = kScratch + 0x668;
    const std::string path = std::string("D:\\") + name;
    std::memcpy(g_memory->Translate(text), path.c_str(), path.size() + 1);
    g_memory->Write16(string + 0, static_cast<uint16_t>(path.size()));
    g_memory->Write16(string + 2, static_cast<uint16_t>(path.size() + 1));
    g_memory->Write32(string + 4, text);
    g_memory->Write32(attributes + 0, 0xFFFFFFFD);  // ObDosDevicesDirectory()
    g_memory->Write32(attributes + 4, string);
    g_memory->Write32(attributes + 8, 0x40);         // OBJ_CASE_INSENSITIVE
    const uint32_t status = Call("NtOpenFile", {handle_out, 0x80100000, attributes, io_status, 1, 0x60});
    return status == 0 ? g_memory->Read32(handle_out) : 0;
}

// ReadFileEx's shape: NtReadFile(file, NULL, NtUserIoApcDispatcher,
// completion, &overlapped, buffer, length, &offset).
uint32_t ReadWithCompletion(uint32_t file, GuestAddress overlapped, uint32_t length, uint32_t offset)
{
    const GuestAddress buffer = kScratch + 0x700;
    const GuestAddress offset_block = kScratch + 0x6F0;
    g_memory->Write32(offset_block, offset);
    g_memory->Write32(offset_block + 4, 0);
    return Call("NtReadFile", {file, 0, rexbox::runtime::KernelThunkVa(232), kCompletionRoutine, overlapped, buffer,
                               length, offset_block});
}

void ProbeUserApc(const std::filesystem::path& game_directory)
{
    g_registry->AddExport("probe", "ProbeCompletionRoutine", &ProbeCompletionRoutine);
    g_registry->BindGuestAddress(kCompletionRoutine, "ProbeCompletionRoutine");
    g_registry->BindGuestAddress(rexbox::runtime::KernelThunkVa(232), "NtUserIoApcDispatcher");
    {
        std::ofstream(game_directory / "apc.bin", std::ios::binary) << "abcdefgh";
    }
    const uint32_t file = OpenGameFile("apc.bin");
    Check(file != 0, "open D:\\apc.bin for the APC checks");
    if (file == 0)
    {
        return;
    }
    const GuestAddress event_out = kScratch + 0x680;
    const GuestAddress overlapped = kScratch + 0x690;
    const GuestAddress zero = Timeout(kScratch + 0x6A0, 0);
    Call("NtCreateEvent", {event_out, 0, 0, 0});
    const uint32_t event = g_memory->Read32(event_out);

    Check(ReadWithCompletion(file, overlapped, 8, 0) == 0 && g_completions == 0,
          "NtReadFile with a completion routine succeeds and runs nothing yet");
    Check(Call("NtWaitForSingleObjectEx", {event, 1, 0, zero}) == kTimeout && g_completions == 0,
          "a non-alertable wait delivers no APC");
    Check(Call("NtWaitForSingleObjectEx", {event, 0, 1, zero}) == kTimeout && g_completions == 0,
          "an alertable kernel-mode wait delivers no user APC");
    Check(Call("NtWaitForSingleObjectEx", {event, 1, 1, zero}) == kUserApc && g_completions == 1,
          "an alertable user-mode wait runs the APC and returns STATUS_USER_APC");
    Check(g_completion_args[0] == 0 && g_completion_args[1] == 8 && g_completion_args[2] == overlapped,
          "the completion routine gets (0, bytes read, the OVERLAPPED)");
    Check(Call("NtWaitForSingleObjectEx", {event, 1, 1, zero}) == kTimeout && g_completions == 1,
          "a delivered APC is not delivered again");

    ReadWithCompletion(file, overlapped, 4, 4);
    Call("NtSetEvent", {event, 0});
    Check(Call("NtWaitForSingleObjectEx", {event, 1, 1, zero}) == 0 && g_completions == 1,
          "a signalled object satisfies the wait before a pending APC");
    Check(Call("KeDelayExecutionThread", {1, 1, zero}) == kUserApc && g_completions == 2 && g_completion_args[1] == 4,
          "an alertable KeDelayExecutionThread runs the pending APC");
    Call("NtClearEvent", {event});

    const GuestAddress handles = kScratch + 0x6B0;
    g_memory->Write32(handles, event);
    ReadWithCompletion(file, overlapped, 8, 0);
    Check(Call("NtWaitForMultipleObjectsEx", {1, handles, 1, 1, 1, zero}) == kUserApc && g_completions == 3,
          "an alertable NtWaitForMultipleObjectsEx runs the APC");
    ReadWithCompletion(file, overlapped, 8, 0);
    Check(Call("NtWaitForSingleObject", {event, 1, zero}) == kUserApc && g_completions == 4,
          "an alertable NtWaitForSingleObject runs the APC");

    Check(ReadWithCompletion(file, overlapped, 8, 100) == 0xC0000011, "a read past the end is STATUS_END_OF_FILE");
    Check(Call("NtWaitForSingleObjectEx", {event, 1, 1, zero}) == kTimeout && g_completions == 4,
          "a read that ends in an error queues no APC");

    g_memory->Write32(overlapped, 0xC0000011);  // Status: STATUS_END_OF_FILE
    g_memory->Write32(overlapped + 4, 5);
    Call("NtUserIoApcDispatcher", {kCompletionRoutine, overlapped, 0});
    Check(g_completions == 5 && g_completion_args[0] == 38 && g_completion_args[1] == 0,
          "NtUserIoApcDispatcher reports an error as (ERROR_HANDLE_EOF, 0 bytes)");
    Call("NtClose", {event});
    Call("NtClose", {file});
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
    ProbeDosDevicesRoot(scratch);
    ProbeUserApc(scratch);

    std::printf("%d check(s), %d failure(s)\n", g_checks, g_failures);
    std::fflush(stdout);
    // The kernel's tick thread runs until the process ends.
    std::_Exit(g_failures == 0 ? 0 : 1);
}
