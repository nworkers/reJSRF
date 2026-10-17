// Opened gamepads: handles, capabilities, state and feedback, read from the
// platform input contract.

#include "xinput_internal.h"
#include "rexbox/platform/input.h"

#include <cstring>

namespace rexbox::hle::xinput
{

namespace
{

constexpr uint32_t kHandleBlockSize = 0x100;

// The XINPUT_GAMEPAD report for a host controller.
void BuildReport(const platform::GamepadSnapshot& pad, uint8_t (&report)[layout::kGamepadSize])
{
    std::memset(report, 0, sizeof(report));
    uint16_t buttons = 0;
    buttons |= pad.dpad_up ? kButtonDpadUp : 0;
    buttons |= pad.dpad_down ? kButtonDpadDown : 0;
    buttons |= pad.dpad_left ? kButtonDpadLeft : 0;
    buttons |= pad.dpad_right ? kButtonDpadRight : 0;
    buttons |= pad.start ? kButtonStart : 0;
    buttons |= pad.back ? kButtonBack : 0;
    buttons |= pad.left_thumb ? kButtonLeftThumb : 0;
    buttons |= pad.right_thumb ? kButtonRightThumb : 0;
    std::memcpy(report + layout::kGamepadButtons, &buttons, 2);
    const uint8_t analog[8] = {pad.a, pad.b, pad.x, pad.y, pad.black, pad.white, pad.left_trigger, pad.right_trigger};
    std::memcpy(report + layout::kGamepadAnalogButtons, analog, 8);
    std::memcpy(report + layout::kGamepadThumbLX, &pad.left_x, 2);
    std::memcpy(report + layout::kGamepadThumbLY, &pad.left_y, 2);
    std::memcpy(report + layout::kGamepadThumbRX, &pad.right_x, 2);
    std::memcpy(report + layout::kGamepadThumbRY, &pad.right_y, 2);
}

OpenDevice* FindDevice(GuestAddress handle)
{
    auto& handles = State().handles;
    const auto it = handles.find(handle);
    return it == handles.end() ? nullptr : &it->second;
}

// HANDLE XInputOpen(PXPP_DEVICE_TYPE DeviceType, DWORD dwPort, DWORD dwSlot,
//     PXINPUT_POLLING_PARAMETERS pPollingParameters)
// Only gamepads exist here. On failure the original also sets the thread's
// last error; the HLE returns NULL without it (logged).
void XInputOpen()
{
    HleCall call;
    const GuestAddress type = call.Arg(0);
    const uint32_t port = call.Arg(1);
    XinputState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    const bool gamepad = type != 0 && type == GamepadType();
    if (!gamepad || port >= kMaxPorts || (state.connected_gamepads & (1u << port)) == 0)
    {
        std::fprintf(stderr, "[xinput] XInputOpen(type %08X, port %u): not connected\n", type, port);
        call.Return(0, 4);
        return;
    }
    const auto block = Heap().AllocateSmall(kHandleBlockSize);
    if (!block)
    {
        call.Return(0, 4);
        return;
    }
    Memory().Fill(*block, 0, kHandleBlockSize);
    OpenDevice device;
    device.port = port;
    state.handles[*block] = device;
    std::fprintf(stderr, "[xinput] XInputOpen(gamepad, port %u, slot %u) -> %08X\n", port, call.Arg(2), *block);
    call.Return(*block, 4);
}

// VOID XInputClose(HANDLE hDevice)
void XInputClose()
{
    HleCall call;
    const GuestAddress handle = call.Arg(0);
    XinputState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (const OpenDevice* device = FindDevice(handle))
    {
        platform::SetGamepadRumble(device->port, 0, 0);
        state.handles.erase(handle);
        Heap().Free(handle);
    }
    call.Return(0, 1);
}

// DWORD XInputGetCapabilities(HANDLE hDevice, PXINPUT_CAPABILITIES pCapabilities)
// Every control is reported as present (all bits and full ranges set).
void XInputGetCapabilities()
{
    HleCall call;
    const GuestAddress handle = call.Arg(0);
    const GuestAddress caps = call.Arg(1);
    XinputState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (FindDevice(handle) == nullptr || caps == 0)
    {
        call.Return(FindDevice(handle) == nullptr ? kErrorDeviceNotConnected : kErrorInvalidParameter, 2);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write8(caps + layout::kCapsSubType, kSubTypeGamepad);
    memory.Write8(caps + 1, 0);
    memory.Write16(caps + 2, 0);
    memory.Fill(caps + layout::kCapsIn, 0xFF, layout::kGamepadSize);
    memory.Write16(caps + layout::kCapsOutLeftMotor, 0xFFFF);
    memory.Write16(caps + layout::kCapsOutRightMotor, 0xFFFF);
    call.Return(kErrorSuccess, 2);
}

// DWORD XInputGetState(HANDLE hDevice, PXINPUT_STATE pState)
// dwPacketNumber changes whenever the report does, as on the console.
void XInputGetState()
{
    HleCall call;
    const GuestAddress handle = call.Arg(0);
    const GuestAddress out = call.Arg(1);
    XinputState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    OpenDevice* device = FindDevice(handle);
    if (device == nullptr)
    {
        call.Return(kErrorDeviceNotConnected, 2);
        return;
    }
    platform::GamepadSnapshot pad;
    platform::ReadGamepad(device->port, &pad);
    uint8_t report[layout::kGamepadSize];
    BuildReport(pad, report);
    if (!device->have_report || std::memcmp(report, device->last_report, sizeof(report)) != 0)
    {
        std::memcpy(device->last_report, report, sizeof(report));
        device->have_report = true;
        ++device->packet_number;
    }
    if (out != 0)
    {
        runtime::GuestMemory& memory = Memory();
        memory.Write32(out + layout::kStatePacketNumber, device->packet_number);
        std::memcpy(memory.Translate(out + layout::kStateGamepad), report, sizeof(report));
    }
    call.Return(kErrorSuccess, 2);
}

// DWORD XInputSetState(HANDLE hDevice, PXINPUT_FEEDBACK pFeedback)
// The rumble goes to the host controller of the device's port
// (docs/design/20261018-i094-gamepad-vibration.md); the request completes at
// once.
void XInputSetState()
{
    HleCall call;
    const GuestAddress handle = call.Arg(0);
    const GuestAddress feedback = call.Arg(1);
    XinputState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    const OpenDevice* device = FindDevice(handle);
    if (device == nullptr)
    {
        call.Return(kErrorDeviceNotConnected, 2);
        return;
    }
    if (feedback != 0)
    {
        const uint16_t left = Memory().Read16(feedback + layout::kFeedbackLeftMotor);
        const uint16_t right = Memory().Read16(feedback + layout::kFeedbackRightMotor);
        platform::SetGamepadRumble(device->port, left, right);
        static bool reported = false;
        if (!reported && (left != 0 || right != 0))
        {
            reported = true;
            std::fprintf(stderr, "[xinput] rumble on port %u: left %u, right %u (first request)\n", device->port,
                         left, right);
        }
        Memory().Write32(feedback + layout::kFeedbackStatus, kErrorSuccess);
        static bool warned = false;
        if (!warned && Memory().Read32(feedback + layout::kFeedbackEvent) != 0)
        {
            warned = true;
            std::fprintf(stderr, "[xinput] XInputSetState with a completion event: the event is not signalled\n");
        }
    }
    call.Return(kErrorSuccess, 2);
}

// DWORD XInputPoll(HANDLE hDevice)
void XInputPoll()
{
    HleCall call;
    call.Return(kErrorSuccess, 1);
}

}  // namespace

void RegisterStateExports(runtime::HleRegistry& registry)
{
    registry.AddExport("xinput", "XInputOpen", &XInputOpen);
    registry.AddExport("xinput", "XInputClose", &XInputClose);
    registry.AddExport("xinput", "XInputGetCapabilities", &XInputGetCapabilities);
    registry.AddExport("xinput", "XInputGetState", &XInputGetState);
    registry.AddExport("xinput", "XInputSetState", &XInputSetState);
    registry.AddExport("xinput", "XInputPoll", &XInputPoll);
}

}  // namespace rexbox::hle::xinput
