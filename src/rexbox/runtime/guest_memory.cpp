#include "rexbox/runtime/guest_memory.h"

#include "rexbox/platform/virtual_memory.h"

namespace rexbox::runtime
{

GuestMemory::~GuestMemory()
{
    Shutdown();
}

bool GuestMemory::Initialize(size_t size)
{
    Shutdown();
    void* block = platform::AllocateVirtualMemory(size, nullptr, false, true);
    if (block == nullptr)
    {
        return false;
    }
    base_ = static_cast<uint8_t*>(block);
    size_ = size;

    const size_t page_size = platform::PageSize();
    page_shift_ = 0;
    while ((size_t{1} << page_shift_) < page_size)
    {
        ++page_shift_;
    }
    // Tracking works when a query of the first page succeeds; the page is
    // only read here, so the probe loses nothing.
    void* probe_page = nullptr;
    size_t probe_count = 0;
    tracks_writes_ = (size_t{1} << page_shift_) == page_size &&
                     platform::TakeWrittenPages(base_, page_size, &probe_page, 1, &probe_count);
    write_version_ = 0;
    page_versions_.assign(tracks_writes_ ? (size + page_size - 1) >> page_shift_ : 0, 0);
    if (probe_count != 0)
    {
        page_versions_[0] = ++write_version_;
    }
    return true;
}

uint64_t GuestMemory::WriteVersion(GuestAddress address, size_t length)
{
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (!tracks_writes_ || length == 0 || !IsValid(address, length))
    {
        return ++write_version_;
    }
    const size_t first = address >> page_shift_;
    const size_t last = (address + length - 1) >> page_shift_;
    const size_t pages = last - first + 1;
    written_pages_.resize(pages);
    size_t found = 0;
    if (!platform::TakeWrittenPages(base_ + (first << page_shift_), pages << page_shift_, written_pages_.data(), pages,
                                    &found))
    {
        found = pages;
        for (size_t i = 0; i < pages; ++i)
        {
            written_pages_[i] = base_ + ((first + i) << page_shift_);
        }
    }
    if (found != 0)
    {
        ++write_version_;
        for (size_t i = 0; i < found; ++i)
        {
            const size_t page = static_cast<size_t>(static_cast<uint8_t*>(written_pages_[i]) - base_) >> page_shift_;
            page_versions_[page] = write_version_;
        }
    }
    uint64_t newest = 0;
    for (size_t page = first; page <= last; ++page)
    {
        newest = page_versions_[page] > newest ? page_versions_[page] : newest;
    }
    return newest;
}

void GuestMemory::Shutdown()
{
    if (base_ != nullptr)
    {
        platform::FreeVirtualMemory(base_, size_);
        base_ = nullptr;
        size_ = 0;
    }
    tracks_writes_ = false;
    page_versions_.clear();
}

bool GuestMemory::IsValid(GuestAddress address, size_t length) const
{
    return base_ != nullptr && address <= size_ && length <= size_ - address;
}

}  // namespace rexbox::runtime
