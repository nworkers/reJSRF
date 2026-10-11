// HLE module: XInput / XPP: gamepad state implemented on host input devices.
//
// The library is statically linked into each title, so its functions are bound
// by symbol name to title-specific guest addresses through a title profile
// (see rexbox/runtime/hle_registry.h).

#ifndef REXBOX_HLE_XINPUT_XINPUT_HLE_H
#define REXBOX_HLE_XINPUT_XINPUT_HLE_H

namespace rexbox::runtime
{
class HleRegistry;
}

namespace rexbox::hle::xinput
{

void RegisterXinputHle(runtime::HleRegistry& registry);

}  // namespace rexbox::hle::xinput

#endif  // REXBOX_HLE_XINPUT_XINPUT_HLE_H
