// HLE module: XAPI (XAPILIB): the Win32-like layer -- files, threads, XInput device enumeration, save games.
//
// The library is statically linked into each title, so its functions are bound
// by symbol name to title-specific guest addresses through a title profile
// (see rexbox/runtime/hle_registry.h).

#ifndef REXBOX_HLE_XAPI_XAPI_HLE_H
#define REXBOX_HLE_XAPI_XAPI_HLE_H

namespace rexbox::runtime
{
class HleRegistry;
}

namespace rexbox::hle::xapi
{

void RegisterXapiHle(runtime::HleRegistry& registry);

}  // namespace rexbox::hle::xapi

#endif  // REXBOX_HLE_XAPI_XAPI_HLE_H
