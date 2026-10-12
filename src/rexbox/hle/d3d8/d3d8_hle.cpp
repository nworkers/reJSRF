#include "rexbox/hle/d3d8/d3d8_hle.h"

#include "d3d8_internal.h"

namespace rexbox::hle::d3d8
{

D3dState& State()
{
    static D3dState state;
    return state;
}

runtime::GuestMemory& Memory()
{
    return *runtime::Env().memory;
}

runtime::GuestHeap& Heap()
{
    return *runtime::Env().heap;
}

void RegisterD3d8Hle(runtime::HleRegistry& registry)
{
    RegisterAdapterExports(registry);
    RegisterCapsExports(registry);
    RegisterResourceExports(registry);
    RegisterStateExports(registry);
    RegisterCreateExports(registry);
    RegisterShaderExports(registry);
    RegisterDrawExports(registry);
    RegisterDeviceExports(registry);
}

void SetD3d8Headless(bool headless)
{
    State().headless = headless;
}

}  // namespace rexbox::hle::d3d8
