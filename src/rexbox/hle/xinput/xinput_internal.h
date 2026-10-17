// Internal to the XInput HLE module: XAPI input constants and structure
// layouts, the module state and the per-area registration functions.
//
// XAPI's own USB host and class-driver stack is not run; these exports stand
// in for the device and input API above it (docs/design/20261012-i015-xinput-hle.md).
// Guest structures are addressed by offset (docs/kb/xbox-xinput.md).

#ifndef REXBOX_HLE_XINPUT_XINPUT_INTERNAL_H
#define REXBOX_HLE_XINPUT_XINPUT_INTERNAL_H

#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_call.h"
#include "rexbox/runtime/hle_registry.h"

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace rexbox::hle::xinput
{

using runtime::GuestAddress;
using runtime::HleCall;

// Win32 error codes XAPI returns.
constexpr uint32_t kErrorSuccess = 0;
constexpr uint32_t kErrorInvalidParameter = 0x57;
constexpr uint32_t kErrorDeviceNotConnected = 0x48F;

constexpr uint32_t kMaxPorts = 4;

// Names of the XDK variables whose addresses are the device types
// (XDEVICE_TYPE_GAMEPAD == &g_DeviceType_Gamepad, ...), bound by the title profile.
constexpr const char* kGamepadTypeVariable = "g_DeviceType_Gamepad";
constexpr const char* kMemoryUnitTypeVariable = "g_DeviceType_MU";

namespace layout
{
// XPP_DEVICE_TYPE (confirmed from the lifted XGetDevices/XGetDeviceChanges).
constexpr uint32_t kTypeCurrentConnected = 0x0;
constexpr uint32_t kTypeChangeConnected = 0x4;
constexpr uint32_t kTypePreviousConnected = 0x8;

// XINPUT_GAMEPAD, 18 bytes.
constexpr uint32_t kGamepadButtons = 0x0;        // WORD
constexpr uint32_t kGamepadAnalogButtons = 0x2;  // BYTE[8]: A B X Y Black White LT RT
constexpr uint32_t kGamepadThumbLX = 0xA;        // SHORT x 4
constexpr uint32_t kGamepadThumbLY = 0xC;
constexpr uint32_t kGamepadThumbRX = 0xE;
constexpr uint32_t kGamepadThumbRY = 0x10;
constexpr uint32_t kGamepadSize = 0x12;

// XINPUT_STATE: DWORD dwPacketNumber, then XINPUT_GAMEPAD.
constexpr uint32_t kStatePacketNumber = 0x0;
constexpr uint32_t kStateGamepad = 0x4;

// XINPUT_CAPABILITIES: BYTE SubType, WORD Reserved, XINPUT_GAMEPAD In, XINPUT_RUMBLE Out.
constexpr uint32_t kCapsSubType = 0x0;
constexpr uint32_t kCapsIn = 0x4;
constexpr uint32_t kCapsOutLeftMotor = 0x16;
constexpr uint32_t kCapsOutRightMotor = 0x18;

// XINPUT_FEEDBACK: the header (DWORD dwStatus, HANDLE hEvent, BYTE
// Reserved[58]), then XINPUT_RUMBLE (WORD wLeftMotorSpeed, WORD wRightMotorSpeed).
constexpr uint32_t kFeedbackStatus = 0x0;
constexpr uint32_t kFeedbackEvent = 0x4;
constexpr uint32_t kFeedbackLeftMotor = 0x42;
constexpr uint32_t kFeedbackRightMotor = 0x44;
}  // namespace layout

constexpr uint8_t kSubTypeGamepad = 1;  // XINPUT_DEVSUBTYPE_GC_GAMEPAD

// wButtons bits.
constexpr uint16_t kButtonDpadUp = 0x0001;
constexpr uint16_t kButtonDpadDown = 0x0002;
constexpr uint16_t kButtonDpadLeft = 0x0004;
constexpr uint16_t kButtonDpadRight = 0x0008;
constexpr uint16_t kButtonStart = 0x0010;
constexpr uint16_t kButtonBack = 0x0020;
constexpr uint16_t kButtonLeftThumb = 0x0040;
constexpr uint16_t kButtonRightThumb = 0x0080;

// An open device handle: a small guest block, so the handle is a valid
// pointer, with the host-side state kept here.
struct OpenDevice
{
    uint32_t port = 0;
    uint32_t packet_number = 0;
    uint8_t last_report[layout::kGamepadSize] = {};
    bool have_report = false;
};

struct XinputState
{
    std::mutex mutex;
    bool initialized = false;
    uint32_t connected_gamepads = 0x1;  // bit per port; port 0 always (keyboard fallback)
    // Gamepads not yet announced. On the console XInitDevices returns before
    // the USB stack has enumerated anything, so the first query sees no
    // device and the gamepads arrive as insertions afterwards; the title
    // depends on it (its device enumeration fails while a change is pending).
    uint32_t arriving_gamepads = 0;
    uint32_t queries = 0;  // XGetDevices / XGetDeviceChanges calls so far
    std::unordered_map<GuestAddress, OpenDevice> handles;
};

XinputState& State();
runtime::GuestMemory& Memory();
runtime::GuestHeap& Heap();
runtime::HleRegistry& Registry();

// The guest address of the gamepad device type, or 0 when the title did not
// bind it.
GuestAddress GamepadType();

void RegisterDeviceExports(runtime::HleRegistry& registry);
void RegisterStateExports(runtime::HleRegistry& registry);

}  // namespace rexbox::hle::xinput

#endif  // REXBOX_HLE_XINPUT_XINPUT_INTERNAL_H
