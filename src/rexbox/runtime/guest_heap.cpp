#include "rexbox/runtime/guest_heap.h"

namespace rexbox::runtime
{

namespace
{

uint32_t PagesFor(uint32_t size)
{
    return (size + GuestHeap::kPageSize - 1) / GuestHeap::kPageSize;
}

}  // namespace

GuestHeap::GuestHeap(size_t memory_size, size_t physical_size)
    : used_(memory_size / kPageSize, false),
      physical_pages_(static_cast<uint32_t>((physical_size == 0 || physical_size > memory_size ? memory_size
                                                                                              : physical_size) /
                                            kPageSize))
{
}

void GuestHeap::Reserve(GuestAddress address, uint32_t size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const uint32_t first = address / kPageSize;
    const uint32_t last = (address + size + kPageSize - 1) / kPageSize;
    MarkRange(first, last - first, true);
}

std::optional<GuestAddress> GuestHeap::Allocate(uint32_t size, uint32_t alignment, bool top_down)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return AllocateLocked(size, alignment, top_down, 0, physical_pages_);
}

std::optional<GuestAddress> GuestHeap::AllocateVirtual(uint32_t size, uint32_t alignment, bool top_down)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (const auto address =
            AllocateLocked(size, alignment, top_down, physical_pages_, static_cast<uint32_t>(used_.size())))
    {
        return address;
    }
    return AllocateLocked(size, alignment, top_down, 0, physical_pages_);
}

std::optional<GuestAddress> GuestHeap::AllocateLocked(uint32_t size, uint32_t alignment, bool top_down,
                                                      uint32_t first_page, uint32_t end_page)
{
    if (size == 0)
    {
        return std::nullopt;
    }
    const uint32_t pages = PagesFor(size);
    const uint32_t step = alignment > kPageSize ? alignment / kPageSize : 1;
    // The first aligned start at or after first_page, and the last one that fits.
    const uint32_t low = (first_page + step - 1) / step * step;
    if (end_page < pages || end_page - pages < low)
    {
        return std::nullopt;
    }
    const uint32_t high = (end_page - pages) / step * step;
    for (uint32_t i = 0; i <= (high - low) / step; ++i)
    {
        const uint32_t first = top_down ? high - i * step : low + i * step;
        if (RangeFree(first, pages))
        {
            MarkRange(first, pages, true);
            const GuestAddress address = first * kPageSize;
            blocks_[address] = pages * kPageSize;
            return address;
        }
    }
    return std::nullopt;
}

std::optional<GuestAddress> GuestHeap::AllocateSmall(uint32_t size)
{
    if (size == 0 || size > kSmallMax)
    {
        return std::nullopt;
    }
    uint32_t slot_size = 16;
    while (slot_size < size)
    {
        slot_size *= 2;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [page, slab] : slabs_)
    {
        if (slab.slot_size != slot_size || slab.used_count == slab.used.size())
        {
            continue;
        }
        for (uint32_t slot = 0; slot < slab.used.size(); ++slot)
        {
            if (!slab.used[slot])
            {
                slab.used[slot] = true;
                ++slab.used_count;
                return page + slot * slot_size;
            }
        }
    }
    // Slab pages come from the top so they stay out of the way of the
    // title's large bottom-up blocks.
    const auto page = AllocateLocked(kPageSize, kPageSize, true, 0, physical_pages_);
    if (!page)
    {
        return std::nullopt;
    }
    Slab& slab = slabs_[*page];
    slab.slot_size = slot_size;
    slab.used.assign(kPageSize / slot_size, false);
    slab.used[0] = true;
    slab.used_count = 1;
    return *page;
}

bool GuestHeap::AllocateAt(GuestAddress address, uint32_t size)
{
    if (size == 0 || address % kPageSize != 0)
    {
        return false;
    }
    const uint32_t first = address / kPageSize;
    const uint32_t pages = PagesFor(size);
    std::lock_guard<std::mutex> lock(mutex_);
    if (first + pages > used_.size() || !RangeFree(first, pages))
    {
        return false;
    }
    MarkRange(first, pages, true);
    blocks_[address] = pages * kPageSize;
    return true;
}

bool GuestHeap::Free(GuestAddress address)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const GuestAddress page = address / kPageSize * kPageSize;
    if (const auto slab = slabs_.find(page); slab != slabs_.end())
    {
        const uint32_t offset = address - page;
        const uint32_t slot = offset / slab->second.slot_size;
        if (offset % slab->second.slot_size != 0 || !slab->second.used[slot])
        {
            return false;
        }
        slab->second.used[slot] = false;
        if (--slab->second.used_count == 0)
        {
            slabs_.erase(slab);
            MarkRange(page / kPageSize, 1, false);
            blocks_.erase(page);
        }
        return true;
    }
    const auto it = blocks_.find(address);
    if (it == blocks_.end())
    {
        return false;
    }
    MarkRange(address / kPageSize, it->second / kPageSize, false);
    blocks_.erase(it);
    return true;
}

uint32_t GuestHeap::BlockSizeContaining(GuestAddress address, GuestAddress* block_start) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const GuestAddress page = address / kPageSize * kPageSize;
    if (const auto slab = slabs_.find(page); slab != slabs_.end())
    {
        const uint32_t slot_size = slab->second.slot_size;
        const uint32_t slot = (address - page) / slot_size;
        if (!slab->second.used[slot])
        {
            return 0;
        }
        if (block_start != nullptr)
        {
            *block_start = page + slot * slot_size;
        }
        return slot_size;
    }
    auto it = blocks_.upper_bound(address);
    if (it == blocks_.begin())
    {
        return 0;
    }
    --it;
    if (address - it->first >= it->second)
    {
        return 0;
    }
    if (block_start != nullptr)
    {
        *block_start = it->first;
    }
    return it->second;
}

uint32_t GuestHeap::free_bytes() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t free_pages = 0;
    for (const bool used : used_)
    {
        free_pages += used ? 0 : 1;
    }
    return free_pages * kPageSize;
}

uint32_t GuestHeap::largest_free_bytes() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t longest = 0;
    uint32_t run = 0;
    for (const bool used : used_)
    {
        run = used ? 0 : run + 1;
        longest = run > longest ? run : longest;
    }
    return longest * kPageSize;
}

bool GuestHeap::RangeFree(uint32_t first_page, uint32_t page_count) const
{
    for (uint32_t page = first_page; page < first_page + page_count; ++page)
    {
        if (used_[page])
        {
            return false;
        }
    }
    return true;
}

void GuestHeap::MarkRange(uint32_t first_page, uint32_t page_count, bool used)
{
    for (uint32_t page = first_page; page < first_page + page_count && page < used_.size(); ++page)
    {
        used_[page] = used;
    }
}

}  // namespace rexbox::runtime
