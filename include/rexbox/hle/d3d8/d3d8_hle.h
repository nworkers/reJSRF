// HLE module: Direct3D 8 (D3D8, XGRAPHC): the title's only path to the NV2A GPU, implemented on a host graphics API.
//
// The library is statically linked into each title, so its functions are bound
// by symbol name to title-specific guest addresses through a title profile
// (see rexbox/runtime/hle_registry.h).

#ifndef REXBOX_HLE_D3D8_D3D8_HLE_H
#define REXBOX_HLE_D3D8_D3D8_HLE_H

namespace rexbox::runtime
{
class HleRegistry;
}

namespace rexbox::hle::d3d8
{

void RegisterD3d8Hle(runtime::HleRegistry& registry);

// Runs without a window: Direct3D_CreateDevice creates no display and the
// drawing exports do nothing (tools, CI). REXBOX_DISPLAY=none does the same.
void SetD3d8Headless(bool headless);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_HLE_H
