// Kernel data exports and kernel time.
//
// Data exports are variables the title reads through its thunk slot, so each
// lives in guest memory and its slot holds its guest address
// (BindKernelImports).

#include "kernel_internal.h"

#include <atomic>
#include <thread>

namespace rexbox::hle::kernel
{

namespace
{

using Clock = std::chrono::steady_clock;

const Clock::time_point g_boot_time = Clock::now();

// NT time of 1970-01-01 (100 ns units since 1601-01-01).
constexpr uint64_t kUnixEpochNt = 116444736000000000ull;

struct DataExport
{
    const char* name;
    uint32_t size;
};

// Every data export of the kernel the HLE knows; sizes are the exported
// object's (docs/kb/xbox-kernel-structures.md).
constexpr DataExport kDataExports[] = {
    {"ExEventObjectType", 0x40},
    {"ExMutantObjectType", 0x40},
    {"ExSemaphoreObjectType", 0x40},
    {"ExTimerObjectType", 0x40},
    {"IoCompletionObjectType", 0x40},
    {"IoDeviceObjectType", 0x40},
    {"IoFileObjectType", 0x40},
    {"ObDirectoryObjectType", 0x40},
    {"ObSymbolicLinkObjectType", 0x40},
    {"PsThreadObjectType", 0x40},
    {"KeTickCount", 4},
    {"KeTimeIncrement", 4},
    {"LaunchDataPage", 4},
    {"HalDiskCachePartitionCount", 4},
    {"HalDiskModelNumber", 0x40},
    {"HalDiskSerialNumber", 0x40},
    {"XboxHardwareInfo", 8},
    {"XboxKrnlVersion", 8},
    {"XboxHDKey", 16},
    {"XboxSignatureKey", 16},
    {"XboxLANKey", 16},
    {"XboxAlternateSignatureKeys", 16 * 16},
    {"XboxEEPROMKey", 16},
    {"HalBootSMCVideoMode", 4},
    {"IdexChannelObject", 0x40},
};

std::atomic<bool> g_ticker_running{false};

void TickerMain()
{
    // KeTickCount is a plain variable the title polls; keep it current.
    for (;;)
    {
        const GuestAddress address = State().tick_count;
        if (address != 0)
        {
            Memory().Write32(address, TickCountMs());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

}  // namespace

uint32_t TickCountMs()
{
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_boot_time).count());
}

uint64_t InterruptTime100ns()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - g_boot_time).count() / 100);
}

uint64_t SystemTimeNt()
{
    const auto since_unix = std::chrono::system_clock::now().time_since_epoch();
    return kUnixEpochNt + static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(since_unix).count() / 100);
}

void InitializeKernelData()
{
    KernelState& state = State();
    uint32_t total = 0;
    for (const DataExport& item : kDataExports)
    {
        total += (item.size + 15u) & ~15u;
    }
    GuestAddress cursor = AllocateZeroed(total);
    for (const DataExport& item : kDataExports)
    {
        state.data_exports[item.name] = cursor;
        cursor += (item.size + 15u) & ~15u;
    }

    runtime::GuestMemory& memory = Memory();
    state.tick_count = state.data_exports["KeTickCount"];
    memory.Write32(state.data_exports["KeTimeIncrement"], 10000);  // 1 ms in 100 ns units
    memory.Write32(state.data_exports["LaunchDataPage"], 0);       // launched from the dashboard
    memory.Write32(state.data_exports["HalDiskCachePartitionCount"], 3);
    memory.Write32(state.data_exports["HalBootSMCVideoMode"], 0);
    // XBOX_HARDWARE_INFO {ULONG Flags; UCHAR GpuRevision, McpRevision, ...}:
    // a retail unit. Values provisional until a title is seen to branch on them.
    memory.Write32(state.data_exports["XboxHardwareInfo"], 0x00000020);
    memory.Write8(state.data_exports["XboxHardwareInfo"] + 4, 0xD3);
    memory.Write8(state.data_exports["XboxHardwareInfo"] + 5, 0xD4);
    // XBOX_KRNL_VERSION {USHORT Major, Minor, Build, Qfe}: 1.0.5838.1.
    const GuestAddress version = state.data_exports["XboxKrnlVersion"];
    memory.Write16(version + 0, 1);
    memory.Write16(version + 2, 0);
    memory.Write16(version + 4, 5838);
    memory.Write16(version + 6, 1);

    if (!g_ticker_running.exchange(true))
    {
        std::thread(&TickerMain).detach();
    }
}

void RegisterDataExports(runtime::HleRegistry& /*registry*/)
{
    // Data exports are not callable; BindKernelImports writes their addresses
    // into the thunk slots instead of binding them.
}

}  // namespace rexbox::hle::kernel
