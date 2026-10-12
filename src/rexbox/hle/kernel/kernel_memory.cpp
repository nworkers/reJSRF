// Memory services: virtual memory, contiguous and system memory, pool.
//
// Every block comes from the runtime GuestHeap, so addresses stay inside guest
// RAM; physical addresses are the identity (design i003 section 4).

#include "kernel_internal.h"

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;

constexpr uint32_t kMemCommit = 0x1000;
constexpr uint32_t kMemReserve = 0x2000;
constexpr uint32_t kMemRelease = 0x8000;
constexpr uint32_t kMemFree = 0x10000;
constexpr uint32_t kMemPrivate = 0x20000;
constexpr uint32_t kMemTopDown = 0x100000;
constexpr uint32_t kPageReadWrite = 0x04;
constexpr uint32_t kAllocationGranularity = 0x10000;

// Protection last set on a block, by its start; reported back by queries.
std::mutex g_protect_mutex;
std::unordered_map<GuestAddress, uint32_t> g_protection;

uint32_t RoundUp(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void RecordProtection(GuestAddress block, uint32_t protect)
{
    std::lock_guard<std::mutex> lock(g_protect_mutex);
    g_protection[block] = protect;
}

uint32_t ProtectionOf(GuestAddress block)
{
    std::lock_guard<std::mutex> lock(g_protect_mutex);
    const auto it = g_protection.find(block);
    return it == g_protection.end() ? kPageReadWrite : it->second;
}

// NTSTATUS NtAllocateVirtualMemory(PVOID* BaseAddress, ULONG_PTR ZeroBits,
//     PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect)
void NtAllocateVirtualMemory()
{
    HleCall call;
    runtime::GuestMemory& memory = Memory();
    const GuestAddress base_io = call.Arg(0);
    const GuestAddress size_io = call.Arg(2);
    const uint32_t type = call.Arg(3);
    const uint32_t protect = call.Arg(4);
    GuestAddress base = memory.Read32(base_io);
    const uint32_t size = memory.Read32(size_io);
    const GuestAddress requested = base;

    if (base != 0)
    {
        // Committing inside an existing reservation needs nothing more: the
        // whole block is already backed.
        GuestAddress block = 0;
        const uint32_t block_size = Heap().BlockSizeContaining(base, &block);
        if (block_size != 0 && base + size <= block + block_size)
        {
            memory.Write32(size_io, RoundUp(size, runtime::GuestHeap::kPageSize));
            memory.Write32(base_io, base & ~(runtime::GuestHeap::kPageSize - 1));
            if (MemoryTraceEnabled())
            {
                std::fprintf(stderr,
                             "[kernel] NtAllocateVirtualMemory commit %08X size %X type %X inside block %08X+%X\n",
                             requested, size, type, block, block_size);
            }
            call.Return(kStatusSuccess, 5);
            return;
        }
        base &= ~(kAllocationGranularity - 1);
        if (!Heap().AllocateAt(base, RoundUp(size, runtime::GuestHeap::kPageSize)))
        {
            std::fprintf(stderr,
                         "[kernel] NtAllocateVirtualMemory at %08X size %X type %X: pages in use (block %08X+%X)\n",
                         requested, size, type, block, block_size);
            call.Return(kStatusNoMemory, 5);
            return;
        }
        memory.Fill(base, 0, RoundUp(size, runtime::GuestHeap::kPageSize));
    }
    else
    {
        base = AllocateZeroed(size, kAllocationGranularity, (type & kMemTopDown) != 0);
        if (base == 0)
        {
            TraceOnce("NtAllocateVirtualMemory", "out of guest memory for " + std::to_string(size) + " bytes");
            call.Return(kStatusNoMemory, 5);
            return;
        }
    }
    RecordProtection(base, protect);
    memory.Write32(base_io, base);
    memory.Write32(size_io, RoundUp(size, runtime::GuestHeap::kPageSize));
    if (MemoryTraceEnabled())
    {
        std::fprintf(stderr, "[kernel] NtAllocateVirtualMemory requested %08X size %X type %X protect %X -> %08X\n",
                     requested, size, type, protect, base);
    }
    call.Return(kStatusSuccess, 5);
}

// NTSTATUS NtFreeVirtualMemory(PVOID* BaseAddress, PSIZE_T RegionSize, ULONG FreeType)
void NtFreeVirtualMemory()
{
    HleCall call;
    const GuestAddress base = Memory().Read32(call.Arg(0));
    if ((call.Arg(2) & kMemRelease) != 0)
    {
        GuestAddress block = 0;
        if (Heap().BlockSizeContaining(base, &block) != 0 && block == base)
        {
            Heap().Free(base);
        }
    }
    // MEM_DECOMMIT keeps the pages; the block stays allocated until released.
    call.Return(kStatusSuccess, 3);
}

// NTSTATUS NtQueryVirtualMemory(PVOID BaseAddress, PMEMORY_BASIC_INFORMATION Buffer)
void NtQueryVirtualMemory()
{
    HleCall call;
    runtime::GuestMemory& memory = Memory();
    const GuestAddress address = call.Arg(0);
    const GuestAddress info = call.Arg(1);
    GuestAddress block = 0;
    const uint32_t block_size = Heap().BlockSizeContaining(address, &block);
    const GuestAddress page = address & ~(runtime::GuestHeap::kPageSize - 1);
    // MEMORY_BASIC_INFORMATION: BaseAddress, AllocationBase, AllocationProtect,
    // RegionSize, State, Protect, Type.
    memory.Write32(info + 0x00, page);
    memory.Write32(info + 0x04, block_size != 0 ? block : 0);
    memory.Write32(info + 0x08, block_size != 0 ? ProtectionOf(block) : 0);
    memory.Write32(info + 0x0C, block_size != 0 ? block + block_size - page : runtime::GuestHeap::kPageSize);
    memory.Write32(info + 0x10, block_size != 0 ? kMemCommit : kMemFree);
    memory.Write32(info + 0x14, block_size != 0 ? ProtectionOf(block) : 0);
    memory.Write32(info + 0x18, block_size != 0 ? kMemPrivate : 0);
    call.Return(kStatusSuccess, 2);
}

// NTSTATUS NtProtectVirtualMemory(PVOID* BaseAddress, PSIZE_T RegionSize,
//     ULONG NewProtect, PULONG OldProtect)
void NtProtectVirtualMemory()
{
    HleCall call;
    const GuestAddress base = Memory().Read32(call.Arg(0));
    GuestAddress block = 0;
    Heap().BlockSizeContaining(base, &block);
    if (call.Arg(3) != 0)
    {
        Memory().Write32(call.Arg(3), ProtectionOf(block));
    }
    RecordProtection(block, call.Arg(2));
    call.Return(kStatusSuccess, 4);
}

GuestAddress AllocateContiguous(uint32_t size, uint32_t alignment)
{
    return AllocateZeroed(size, alignment < runtime::GuestHeap::kPageSize ? runtime::GuestHeap::kPageSize : alignment);
}

// PVOID MmAllocateContiguousMemory(SIZE_T NumberOfBytes)
void MmAllocateContiguousMemory()
{
    HleCall call;
    call.Return(AllocateContiguous(call.Arg(0), runtime::GuestHeap::kPageSize), 1);
}

// PVOID MmAllocateContiguousMemoryEx(SIZE_T NumberOfBytes, ULONG_PTR LowestAcceptableAddress,
//     ULONG_PTR HighestAcceptableAddress, ULONG_PTR Alignment, ULONG Protect)
void MmAllocateContiguousMemoryEx()
{
    HleCall call;
    const GuestAddress address = AllocateContiguous(call.Arg(0), call.Arg(3));
    if (address != 0)
    {
        RecordProtection(address, call.Arg(4));
    }
    call.Return(address, 5);
}

// VOID MmFreeContiguousMemory(PVOID BaseAddress)
void MmFreeContiguousMemory()
{
    HleCall call;
    Heap().Free(call.Arg(0));
    call.Return(0, 1);
}

// VOID MmPersistContiguousMemory(PVOID BaseAddress, SIZE_T NumberOfBytes, BOOLEAN Persist)
void MmPersistContiguousMemory()
{
    HleCall().Return(0, 3);
}

// PVOID MmAllocateSystemMemory(SIZE_T NumberOfBytes, ULONG Protect)
void MmAllocateSystemMemory()
{
    HleCall call;
    const GuestAddress address = AllocateZeroed(call.Arg(0));
    if (address != 0)
    {
        RecordProtection(address, call.Arg(1));
    }
    call.Return(address, 2);
}

// ULONG MmFreeSystemMemory(PVOID BaseAddress, SIZE_T NumberOfBytes)
void MmFreeSystemMemory()
{
    HleCall call;
    GuestAddress block = 0;
    const uint32_t size = Heap().BlockSizeContaining(call.Arg(0), &block);
    Heap().Free(call.Arg(0));
    call.Return(size / runtime::GuestHeap::kPageSize, 2);
}

// SIZE_T MmQueryAllocationSize(PVOID BaseAddress)
void MmQueryAllocationSize()
{
    HleCall call;
    GuestAddress block = 0;
    call.Return(Heap().BlockSizeContaining(call.Arg(0), &block), 1);
}

// ULONG_PTR MmGetPhysicalAddress(PVOID BaseAddress)
void MmGetPhysicalAddress()
{
    HleCall call;
    call.Return(call.Arg(0), 1);
}

// ULONG MmQueryAddressProtect(PVOID VirtualAddress)
void MmQueryAddressProtect()
{
    HleCall call;
    GuestAddress block = 0;
    Heap().BlockSizeContaining(call.Arg(0), &block);
    call.Return(ProtectionOf(block), 1);
}

// VOID MmSetAddressProtect(PVOID BaseAddress, ULONG NumberOfBytes, ULONG NewProtect)
void MmSetAddressProtect()
{
    HleCall call;
    GuestAddress block = 0;
    Heap().BlockSizeContaining(call.Arg(0), &block);
    RecordProtection(block, call.Arg(2));
    call.Return(0, 3);
}

// VOID MmLockUnlockBufferPages(PVOID BaseAddress, SIZE_T NumberOfBytes, BOOLEAN UnlockPages)
void MmLockUnlockBufferPages()
{
    HleCall().Return(0, 3);
}

// VOID MmLockUnlockPhysicalPage(ULONG_PTR PhysicalAddress, BOOLEAN UnlockPage)
void MmLockUnlockPhysicalPage()
{
    HleCall().Return(0, 2);
}

// PVOID MmClaimGpuInstanceMemory(SIZE_T NumberOfBytes, SIZE_T* NumberOfPaddingBytes)
//
// The GPU's instance memory sits at the top of physical RAM on the Xbox. The
// D3D8 HLE will not use it; hand back a real block so D3D's own setup, while
// it still runs, writes somewhere harmless.
void MmClaimGpuInstanceMemory()
{
    HleCall call;
    const uint32_t size = call.Arg(0);
    if (call.Arg(1) != 0)
    {
        Memory().Write32(call.Arg(1), 0);
    }
    if (size == 0xFFFFFFFF)
    {
        // A query for the end of instance memory.
        call.Return(static_cast<uint32_t>(Memory().size()), 2);
        return;
    }
    call.Return(AllocateZeroed(size, runtime::GuestHeap::kPageSize, true), 2);
}

// PVOID ExAllocatePool(SIZE_T NumberOfBytes)
void ExAllocatePool()
{
    HleCall call;
    call.Return(AllocateZeroed(call.Arg(0), 16), 1);
}

// PVOID ExAllocatePoolWithTag(SIZE_T NumberOfBytes, ULONG Tag)
void ExAllocatePoolWithTag()
{
    HleCall call;
    call.Return(AllocateZeroed(call.Arg(0), 16), 2);
}

// VOID ExFreePool(PVOID P)
void ExFreePool()
{
    HleCall call;
    Heap().Free(call.Arg(0));
    call.Return(0, 1);
}

// ULONG ExQueryPoolBlockSize(PVOID PoolBlock)
void ExQueryPoolBlockSize()
{
    HleCall call;
    GuestAddress block = 0;
    call.Return(Heap().BlockSizeContaining(call.Arg(0), &block), 1);
}

}  // namespace

void RegisterMemoryExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "NtAllocateVirtualMemory", &NtAllocateVirtualMemory);
    registry.AddExport("kernel", "NtFreeVirtualMemory", &NtFreeVirtualMemory);
    registry.AddExport("kernel", "NtQueryVirtualMemory", &NtQueryVirtualMemory);
    registry.AddExport("kernel", "NtProtectVirtualMemory", &NtProtectVirtualMemory);
    registry.AddExport("kernel", "MmAllocateContiguousMemory", &MmAllocateContiguousMemory);
    registry.AddExport("kernel", "MmAllocateContiguousMemoryEx", &MmAllocateContiguousMemoryEx);
    registry.AddExport("kernel", "MmFreeContiguousMemory", &MmFreeContiguousMemory);
    registry.AddExport("kernel", "MmPersistContiguousMemory", &MmPersistContiguousMemory);
    registry.AddExport("kernel", "MmAllocateSystemMemory", &MmAllocateSystemMemory);
    registry.AddExport("kernel", "MmFreeSystemMemory", &MmFreeSystemMemory);
    registry.AddExport("kernel", "MmQueryAllocationSize", &MmQueryAllocationSize);
    registry.AddExport("kernel", "MmGetPhysicalAddress", &MmGetPhysicalAddress);
    registry.AddExport("kernel", "MmQueryAddressProtect", &MmQueryAddressProtect);
    registry.AddExport("kernel", "MmSetAddressProtect", &MmSetAddressProtect);
    registry.AddExport("kernel", "MmLockUnlockBufferPages", &MmLockUnlockBufferPages);
    registry.AddExport("kernel", "MmLockUnlockPhysicalPage", &MmLockUnlockPhysicalPage);
    registry.AddExport("kernel", "MmClaimGpuInstanceMemory", &MmClaimGpuInstanceMemory);
    registry.AddExport("kernel", "ExAllocatePool", &ExAllocatePool);
    registry.AddExport("kernel", "ExAllocatePoolWithTag", &ExAllocatePoolWithTag);
    registry.AddExport("kernel", "ExFreePool", &ExFreePool);
    registry.AddExport("kernel", "ExQueryPoolBlockSize", &ExQueryPoolBlockSize);
}

}  // namespace rexbox::hle::kernel
