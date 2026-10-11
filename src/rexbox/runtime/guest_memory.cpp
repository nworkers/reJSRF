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
    void* block = platform::AllocateVirtualMemory(size, nullptr, false);
    if (block == nullptr)
    {
        return false;
    }
    base_ = static_cast<uint8_t*>(block);
    size_ = size;
    return true;
}

void GuestMemory::Shutdown()
{
    if (base_ != nullptr)
    {
        platform::FreeVirtualMemory(base_, size_);
        base_ = nullptr;
        size_ = 0;
    }
}

bool GuestMemory::IsValid(GuestAddress address, size_t length) const
{
    return base_ != nullptr && address <= size_ && length <= size_ - address;
}

}  // namespace rexbox::runtime
