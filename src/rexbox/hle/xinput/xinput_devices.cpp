// Device enumeration and memory units. XInitDevices stands in for XAPI's USB
// stack start-up and starts host input. XGetDevices and XGetDeviceChanges
// keep the original's semantics on the title's XPP_DEVICE_TYPE (confirmed
// from the lifted bodies, docs/kb/xbox-xinput.md) and add the console's
// timing: gamepads arrive after the first query, as insertions.

#include "xinput_internal.h"
#include "rexbox/platform/input.h"

#include <string>

namespace rexbox::hle::xinput
{

namespace
{

// VOID XInitDevices(DWORD dwPreallocTypeCount, PXDEVICE_PREALLOC_TYPE PreallocTypes)
void XInitDevices()
{
    HleCall call;
    XinputState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.initialized)
    {
        state.initialized = true;
        std::string error;
        if (!platform::InitializeInput(&error))
        {
            std::fprintf(stderr, "[xinput] host gamepads unavailable (%s); keyboard only\n", error.c_str());
        }
        const GuestAddress gamepad = GamepadType();
        if (gamepad != 0)
        {
            // Nothing enumerated yet; the gamepads arrive after the first query.
            runtime::GuestMemory& memory = Memory();
            memory.Write32(gamepad + layout::kTypeCurrentConnected, 0);
            memory.Write32(gamepad + layout::kTypeChangeConnected, 0);
            memory.Write32(gamepad + layout::kTypePreviousConnected, 0);
            state.arriving_gamepads = state.connected_gamepads;
        }
        else
        {
            std::fprintf(stderr, "[xinput] the title bound no g_DeviceType_Gamepad; no gamepad will be reported\n");
        }
        std::fprintf(stderr, "[xinput] XInitDevices(%u preallocated types): gamepad ports %X\n", call.Arg(0),
                     state.connected_gamepads);
    }
    call.Return(0, 2);
}

// Called with the state lock held, at the start of each query: once a query
// has completed, the pending gamepads are connected and their change bits set.
void AnnounceArrivals(XinputState& state, GuestAddress type)
{
    const GuestAddress gamepad = GamepadType();
    if (type == 0 || type != gamepad || state.arriving_gamepads == 0 || state.queries == 0)
    {
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write32(gamepad + layout::kTypeCurrentConnected,
                   memory.Read32(gamepad + layout::kTypeCurrentConnected) | state.arriving_gamepads);
    memory.Write32(gamepad + layout::kTypeChangeConnected,
                   memory.Read32(gamepad + layout::kTypeChangeConnected) | state.arriving_gamepads);
    std::fprintf(stderr, "[xinput] gamepad ports %X arrive\n", state.arriving_gamepads);
    state.arriving_gamepads = 0;
}

// DWORD XGetDevices(PXPP_DEVICE_TYPE DeviceType)
// The original: returns CurrentConnected and sets Change = 0, Previous = Current.
void XGetDevices()
{
    HleCall call;
    const GuestAddress type = call.Arg(0);
    XinputState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    AnnounceArrivals(state, type);
    uint32_t current = 0;
    if (type != 0)
    {
        runtime::GuestMemory& memory = Memory();
        current = memory.Read32(type + layout::kTypeCurrentConnected);
        memory.Write32(type + layout::kTypeChangeConnected, 0);
        memory.Write32(type + layout::kTypePreviousConnected, current);
    }
    ++state.queries;
    call.Return(current, 1);
}

// BOOL XGetDeviceChanges(PXPP_DEVICE_TYPE DeviceType, PDWORD pdwInsertions, PDWORD pdwRemovals)
// The original: with no change pending, both outputs 0 and FALSE; otherwise
// insertions = Current & ~Previous | (Change & Previous & Current), removals =
// ~Current & Previous | (Change & Previous & Current), then Change = 0,
// Previous = Current, TRUE.
void XGetDeviceChanges()
{
    HleCall call;
    const GuestAddress type = call.Arg(0);
    const GuestAddress insertions_out = call.Arg(1);
    const GuestAddress removals_out = call.Arg(2);
    XinputState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    AnnounceArrivals(state, type);
    runtime::GuestMemory& memory = Memory();
    uint32_t result = 0;
    uint32_t insertions = 0;
    uint32_t removals = 0;
    if (type != 0)
    {
        const uint32_t current = memory.Read32(type + layout::kTypeCurrentConnected);
        const uint32_t change = memory.Read32(type + layout::kTypeChangeConnected);
        const uint32_t previous = memory.Read32(type + layout::kTypePreviousConnected);
        if (change != 0)
        {
            const uint32_t bounced = change & previous & current;
            insertions = (current & ~previous) | bounced;
            removals = (~current & previous) | bounced;
            memory.Write32(type + layout::kTypeChangeConnected, 0);
            memory.Write32(type + layout::kTypePreviousConnected, current);
            result = 1;
        }
    }
    if (insertions_out != 0)
    {
        memory.Write32(insertions_out, insertions);
    }
    if (removals_out != 0)
    {
        memory.Write32(removals_out, removals);
    }
    ++state.queries;
    call.Return(result, 3);
}

// DWORD XMountMUA(DWORD dwPort, DWORD dwSlot, PCHAR pchDrive)
// No memory units are attached.
void XMountMUA()
{
    HleCall call;
    call.Return(kErrorDeviceNotConnected, 3);
}

// DWORD XUnmountMU(DWORD dwPort, DWORD dwSlot)
void XUnmountMU()
{
    HleCall call;
    call.Return(kErrorDeviceNotConnected, 2);
}

}  // namespace

void RegisterDeviceExports(runtime::HleRegistry& registry)
{
    registry.AddExport("xinput", "XInitDevices", &XInitDevices);
    registry.AddExport("xinput", "XGetDevices", &XGetDevices);
    registry.AddExport("xinput", "XGetDeviceChanges", &XGetDeviceChanges);
    registry.AddExport("xinput", "XMountMUA", &XMountMUA);
    registry.AddExport("xinput", "XUnmountMU", &XUnmountMU);
}

}  // namespace rexbox::hle::xinput
