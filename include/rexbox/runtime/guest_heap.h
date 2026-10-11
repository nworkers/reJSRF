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

    // Manages [0, memory_size). Nothing is reserved yet.
    explicit GuestHeap(size_t memory_size);

    // Marks a range as permanently in use (the image, low memory).
    void Reserve(GuestAddress address, uint32_t size);

    // Allocates `size` bytes (rounded up to pages) aligned to `alignment`
    // (a power of two, at least a page), zero-filled by the caller's memory.
    // Searches from the top when `top_down`. Returns std::nullopt when full.
    std::optional<GuestAddress> Allocate(uint32_t size, uint32_t alignment, bool top_down);

    // Allocates exactly [address, address + size) if every page is free.
    bool AllocateAt(GuestAddress address, uint32_t size);

    // Releases a block returned by Allocate/AllocateAt. Returns false if
    // `address` does not start one.
    bool Free(GuestAddress address);

    // Size of the block starting at or containing `address`, 0 if none.
    uint32_t BlockSizeContaining(GuestAddress address, GuestAddress* block_start) const;

    uint32_t free_bytes() const;

private:
    bool RangeFree(uint32_t first_page, uint32_t page_count) const;
    void MarkRange(uint32_t first_page, uint32_t page_count, bool used);

    mutable std::mutex mutex_;
    std::vector<bool> used_;                     // one entry per page
    std::map<GuestAddress, uint32_t> blocks_;    // start -> size in bytes
};

}  // namespace rexbox::runtime

#endif  // REXBOX_RUNTIME_GUEST_HEAP_H
