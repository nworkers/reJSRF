// Internal to the kernel HLE module: state and helpers shared by its files.

#ifndef REXBOX_HLE_KERNEL_KERNEL_INTERNAL_H
#define REXBOX_HLE_KERNEL_KERNEL_INTERNAL_H

#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_cpu.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_call.h"
#include "rexbox/runtime/hle_registry.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rexbox::hle::kernel
{

using runtime::GuestAddress;

// NTSTATUS values used by the HLE.
constexpr uint32_t kStatusSuccess = 0x00000000;
constexpr uint32_t kStatusTimeout = 0x00000102;
constexpr uint32_t kStatusPending = 0x00000103;
constexpr uint32_t kStatusObjectNameExists = 0x40000000;
constexpr uint32_t kStatusNoMoreFiles = 0x80000006;
constexpr uint32_t kStatusUnsuccessful = 0xC0000001;
constexpr uint32_t kStatusNotImplemented = 0xC0000002;
constexpr uint32_t kStatusInvalidHandle = 0xC0000008;
constexpr uint32_t kStatusInvalidParameter = 0xC000000D;
constexpr uint32_t kStatusNoSuchFile = 0xC000000F;
constexpr uint32_t kStatusEndOfFile = 0xC0000011;
constexpr uint32_t kStatusNoMemory = 0xC0000017;
constexpr uint32_t kStatusAccessDenied = 0xC0000022;
constexpr uint32_t kStatusObjectNameNotFound = 0xC0000034;
constexpr uint32_t kStatusObjectNameCollision = 0xC0000035;
constexpr uint32_t kStatusObjectPathNotFound = 0xC000003A;
constexpr uint32_t kStatusMutantNotOwned = 0xC0000046;
constexpr uint32_t kStatusSemaphoreLimitExceeded = 0xC0000047;

// Xbox structure layout (docs/kb/xbox-kernel-structures.md).
namespace layout
{
// KPCR
constexpr uint32_t kPcrExceptionList = 0x00;
constexpr uint32_t kPcrStackBase = 0x04;
constexpr uint32_t kPcrStackLimit = 0x08;
constexpr uint32_t kPcrSelf = 0x18;
constexpr uint32_t kPcrSelfPcr = 0x1C;
constexpr uint32_t kPcrPrcb = 0x20;
constexpr uint32_t kPcrIrql = 0x24;
constexpr uint32_t kPcrPrcbData = 0x28;
constexpr uint32_t kPcrCurrentThread = 0x28;  // PrcbData.CurrentThread
constexpr uint32_t kPcrBlockSize = 0x400;     // covers reads up to Prcb + 0x250 and beyond
// DISPATCHER_HEADER
constexpr uint32_t kHeaderType = 0x00;
constexpr uint32_t kHeaderSize = 0x02;
constexpr uint32_t kHeaderSignalState = 0x04;
constexpr uint32_t kHeaderWaitListHead = 0x08;
// KTHREAD / ETHREAD
constexpr uint32_t kThreadStackBase = 0x1C;
constexpr uint32_t kThreadStackLimit = 0x20;
constexpr uint32_t kThreadKernelStack = 0x24;
constexpr uint32_t kThreadTlsData = 0x28;
constexpr uint32_t kThreadBasePriority = 0x70;
constexpr uint32_t kThreadDisableBoost = 0x73;
constexpr uint32_t kThreadHasTerminated = 0x77;
constexpr uint32_t kEthreadExitStatus = 0x120;
constexpr uint32_t kEthreadUniqueThread = 0x12C;
constexpr uint32_t kEthreadSize = 0x140;
}  // namespace layout

// Dispatcher object types (DISPATCHER_HEADER.Type).
enum class ObjectType : uint8_t
{
    kNotificationEvent = 0,
    kSynchronizationEvent = 1,
    kMutant = 2,
    kSemaphore = 5,
    kThread = 6,
    kNotificationTimer = 8,
    kSynchronizationTimer = 9,
};

// What a handle refers to.
struct KernelObject
{
    enum class Kind
    {
        kThread,
        kEvent,
        kSemaphore,
        kMutant,
        kTimer,
        kFile,
        kDirectory,
        kSymbolicLink,
        kDevice,
    };
    Kind kind = Kind::kEvent;
    GuestAddress object = 0;   // guest address of the dispatcher object, if any
    std::string name;
    // Files, directories and links: the Xbox name and its host counterpart.
    std::string xbox_path;
    std::filesystem::path host_path;
    std::shared_ptr<std::fstream> stream;
    uint64_t position = 0;
    bool read_only = false;
    bool volume = false;  // the partition itself: also takes raw sector I/O
    bool delete_on_close = false;
    // Directory enumeration (NtQueryDirectoryFile).
    bool directory_listed = false;
    std::vector<std::filesystem::path> directory_entries;
    size_t directory_cursor = 0;
};

struct KernelThread;

// Global kernel state. The dispatcher lock guards every dispatcher object's
// guest SignalState and the handle table; `signalled` wakes waiters.
struct KernelState
{
    std::mutex dispatcher;
    std::condition_variable signalled;
    std::unordered_map<uint32_t, std::shared_ptr<KernelObject>> handles;
    uint32_t next_handle = 0x100;
    std::unordered_map<std::string, uint32_t> data_exports;  // name -> guest address
    std::unordered_map<std::string, std::shared_ptr<KernelObject>> named_objects;
    GuestAddress tick_count = 0;  // KeTickCount
    uint32_t next_thread_id = 1;
    std::unordered_map<uint32_t, std::shared_ptr<KernelThread>> threads;  // by id
};

KernelState& State();

inline runtime::GuestMemory& Memory()
{
    return *runtime::Env().memory;
}

inline runtime::GuestHeap& Heap()
{
    return *runtime::Env().heap;
}

// Allocates zeroed guest memory, or 0 when full.
GuestAddress AllocateZeroed(uint32_t size, uint32_t alignment = runtime::GuestHeap::kPageSize, bool top_down = false);

// Adds a handle for `object`; the dispatcher lock must be held.
uint32_t InsertHandleLocked(std::shared_ptr<KernelObject> object);
std::shared_ptr<KernelObject> LookupHandle(uint32_t handle);

// Logs one line per distinct (function, detail) the first time it happens.
void TraceOnce(const char* function, const std::string& detail);

// Registration of each file's exports.
void RegisterDataExports(runtime::HleRegistry& registry);
void RegisterThreadExports(runtime::HleRegistry& registry);
void RegisterSyncExports(runtime::HleRegistry& registry);
void RegisterMemoryExports(runtime::HleRegistry& registry);
void RegisterObjectExports(runtime::HleRegistry& registry);
void RegisterRtlExports(runtime::HleRegistry& registry);
void RegisterHalExports(runtime::HleRegistry& registry);
void RegisterFileExports(runtime::HleRegistry& registry);
void RegisterCryptoExports(runtime::HleRegistry& registry);

// A host-side record of a guest thread.
struct KernelThread
{
    uint32_t id = 0;
    GuestAddress ethread = 0;
    GuestAddress kpcr = 0;
    GuestAddress stack_block = 0;  // stack + TLS allocation
    // Suspension: the thread waits in its start hook while suspend_count > 0.
    std::mutex gate;
    std::condition_variable resumed;
    int suspend_count = 0;
};

// The calling host thread's guest thread, or nullptr outside guest threads.
KernelThread* CurrentThread();

using Deadline = std::optional<std::chrono::steady_clock::time_point>;

// Converts a guest PLARGE_INTEGER timeout (100 ns units; negative relative,
// positive absolute system time, null pointer = wait forever) to a deadline.
Deadline DeadlineFromTimeout(GuestAddress timeout_pointer);

// Current system time as an NT time (100 ns since 1601-01-01).
uint64_t SystemTimeNt();
// Milliseconds since boot (KeTickCount) and 100 ns interrupt time.
uint32_t TickCountMs();
uint64_t InterruptTime100ns();

// Kernel data area: allocates the data exports and starts the tick thread.
void InitializeKernelData();

// Signals a thread object at exit (dispatcher lock taken inside).
void SignalThreadExit(GuestAddress ethread, uint32_t exit_status);

}  // namespace rexbox::hle::kernel

#endif  // REXBOX_HLE_KERNEL_KERNEL_INTERNAL_H
