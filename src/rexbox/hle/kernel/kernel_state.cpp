#include "kernel_internal.h"

#include <set>
#include <utility>

namespace rexbox::hle::kernel
{

KernelState& State()
{
    static KernelState state;
    return state;
}

GuestAddress AllocateZeroed(uint32_t size, uint32_t alignment, bool top_down)
{
    const std::optional<GuestAddress> address = Heap().Allocate(size, alignment, top_down);
    if (!address)
    {
        return 0;
    }
    Memory().Fill(*address, 0, (size + runtime::GuestHeap::kPageSize - 1) & ~(runtime::GuestHeap::kPageSize - 1));
    return *address;
}

uint32_t InsertHandleLocked(std::shared_ptr<KernelObject> object)
{
    KernelState& state = State();
    const uint32_t handle = state.next_handle;
    state.next_handle += 4;
    state.handles.emplace(handle, std::move(object));
    return handle;
}

std::shared_ptr<KernelObject> LookupHandle(uint32_t handle)
{
    KernelState& state = State();
    std::lock_guard<std::mutex> lock(state.dispatcher);
    const auto it = state.handles.find(handle);
    return it == state.handles.end() ? nullptr : it->second;
}

void TraceOnce(const char* function, const std::string& detail)
{
    static std::mutex mutex;
    static std::set<std::pair<std::string, std::string>> seen;
    std::lock_guard<std::mutex> lock(mutex);
    if (seen.emplace(function, detail).second)
    {
        std::fprintf(stderr, "[kernel] %s %s\n", function, detail.c_str());
    }
}

}  // namespace rexbox::hle::kernel
