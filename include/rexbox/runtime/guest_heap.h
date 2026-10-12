// Page allocator over guest memory.
//
// Every block the HLE hands to the guest -- virtual memory, contiguous
// memory, pool, kernel data, thread stacks -- comes from here, so all guest
// addresses stay inside the mapped guest RAM (docs/design/20261012-i003-kernel-hle.md section 4).

#ifndef REXBOX_RUNTIME_GUEST_HEAP_H
#define REXBOX_RUNTIME_GUEST_HEAP_H

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

namespace rexbox::runtime
{

class GuestHeap
{
public:
    static constexpr uint32_t kPageSize = 0x1000;

    // Manages [0, memory_size). [0, physical_size) stands for the Xbox's
    // physical RAM and serves Allocate and AllocateSmall; the rest is extra
    // address space only AllocateVirtual reaches. Nothing is reserved yet.
    explicit GuestHeap(size_t memory_size, size_t physical_size = 0);

    // Marks a range as permanently in use (the image, low memory).
    void Reserve(GuestAddress address, uint32_t size);

    // Allocates `size` bytes (rounded up to pages) aligned to `alignment`
    // (a power of two, at least a page), zero-filled by the caller's memory.
    // Searches from the top when `top_down`. Returns std::nullopt when full.
    std::optional<GuestAddress> Allocate(uint32_t size, uint32_t alignment, bool top_down);

    // As Allocate, for virtual memory: tries the address space above physical
    // RAM first, then falls back to physical RAM.
    std::optional<GuestAddress> AllocateVirtual(uint32_t size, uint32_t alignment, bool top_down);

    // Allocates a small object (1..kSmallMax bytes) from a shared page,
    // aligned to its size class (a power of two, at least 16 bytes). HLE
    // objects of a few dozen bytes each used to take a whole page, which
    // the 64 MiB guest cannot spare. Zero-filled by the caller's memory;
    // released with Free. Returns std::nullopt when full or too large.
    static constexpr uint32_t kSmallMax = 2048;
    std::optional<GuestAddress> AllocateSmall(uint32_t size);

    // Allocates exactly [address, address + size) if every page is free.
    bool AllocateAt(GuestAddress address, uint32_t size);

    // Releases a block returned by Allocate/AllocateAt, or an object from
    // AllocateSmall. Returns false if `address` starts neither.
    bool Free(GuestAddress address);

    // Size of the block starting at or containing `address`, 0 if none.
    uint32_t BlockSizeContaining(GuestAddress address, GuestAddress* block_start) const;

    uint32_t free_bytes() const;

    // The longest run of free pages, in bytes: the largest block that can
    // still be allocated at page alignment.
    uint32_t largest_free_bytes() const;

private:
    // One page carved into equal slots for AllocateSmall.
    struct Slab
    {
        uint32_t slot_size = 0;
        uint32_t used_count = 0;
        std::vector<bool> used;
    };

    // Searches pages [first_page, end_page).
    std::optional<GuestAddress> AllocateLocked(uint32_t size, uint32_t alignment, bool top_down, uint32_t first_page,
                                               uint32_t end_page);
    bool RangeFree(uint32_t first_page, uint32_t page_count) const;
    void MarkRange(uint32_t first_page, uint32_t page_count, bool used);

    mutable std::mutex mutex_;
    std::vector<bool> used_;                     // one entry per page
    uint32_t physical_pages_ = 0;                // pages of physical RAM, from 0
    std::map<GuestAddress, uint32_t> blocks_;    // start -> size in bytes
    std::map<GuestAddress, Slab> slabs_;         // slab page -> its slots (the page is also in blocks_)
};

}  // namespace rexbox::runtime

#endif  // REXBOX_RUNTIME_GUEST_HEAP_H
