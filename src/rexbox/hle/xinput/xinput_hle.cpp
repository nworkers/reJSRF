#include "rexbox/hle/xinput/xinput_hle.h"

#include "xinput_internal.h"

namespace rexbox::hle::xinput
{

XinputState& State()
{
    static XinputState state;
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

runtime::HleRegistry& Registry()
{
    return *runtime::Env().registry;
}

GuestAddress GamepadType()
{
    return Registry().FindVariable(kGamepadTypeVariable);
}

void RegisterXinputHle(runtime::HleRegistry& registry)
{
    RegisterDeviceExports(registry);
    RegisterStateExports(registry);
}

}  // namespace rexbox::hle::xinput
