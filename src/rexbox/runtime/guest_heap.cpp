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

GuestHeap::GuestHeap(size_t memory_size) : used_(memory_size / kPageSize, false)
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
    if (size == 0)
    {
        return std::nullopt;
    }
    const uint32_t pages = PagesFor(size);
    const uint32_t step = alignment > kPageSize ? alignment / kPageSize : 1;
    const uint32_t total = static_cast<uint32_t>(used_.size());
    std::lock_guard<std::mutex> lock(mutex_);
    if (pages > total)
    {
        return std::nullopt;
    }
    const uint32_t last_start = (total - pages) / step * step;
    for (uint32_t i = 0; i <= last_start / step; ++i)
    {
        const uint32_t first = top_down ? last_start - i * step : i * step;
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
