// Internal to the D3D8 HLE module: Xbox D3D8 constants and structure
// layouts, the module state and the per-area registration functions.
//
// Guest structures are addressed by offset through GuestMemory, never cast to
// host structures (docs/kb/xbox-d3d8-api.md has the layouts).

#ifndef REXBOX_HLE_D3D8_D3D8_INTERNAL_H
#define REXBOX_HLE_D3D8_D3D8_INTERNAL_H

#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_cpu.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_call.h"
#include "rexbox/runtime/hle_registry.h"

#include <cstdint>
#include <cstdio>
#include <mutex>

namespace rexbox::hle::d3d8
{

using runtime::GuestAddress;
using runtime::HleCall;

// HRESULTs the Xbox D3D8 library returns.
constexpr uint32_t kD3dOk = 0x00000000;
constexpr uint32_t kD3dErrNotAvailable = 0x8876086A;
constexpr uint32_t kD3dErrInvalidDevice = 0x8876086B;
constexpr uint32_t kD3dErrInvalidCall = 0x8876086C;

constexpr uint32_t kDevTypeHal = 1;

// Resource types (D3DRESOURCETYPE).
constexpr uint32_t kRTypeSurface = 1;
constexpr uint32_t kRTypeVolume = 2;
constexpr uint32_t kRTypeTexture = 3;
constexpr uint32_t kRTypeVolumeTexture = 4;
constexpr uint32_t kRTypeCubeTexture = 5;
constexpr uint32_t kRTypeVertexBuffer = 6;
constexpr uint32_t kRTypeIndexBuffer = 7;
constexpr uint32_t kRTypePushBuffer = 8;
constexpr uint32_t kRTypePalette = 9;

// Usage flags (D3DUSAGE_*).
constexpr uint32_t kUsageRenderTarget = 0x00000001;
constexpr uint32_t kUsageDepthStencil = 0x00000002;

// Xbox structure layouts, in bytes.
namespace layout
{
// D3DDISPLAYMODE. The Xbox structure carries Flags before Format (measured:
// the library writes {Width, Height, RefreshRate, Flags, Format}).
constexpr uint32_t kDisplayModeWidth = 0x00;
constexpr uint32_t kDisplayModeHeight = 0x04;
constexpr uint32_t kDisplayModeRefreshRate = 0x08;
constexpr uint32_t kDisplayModeFlags = 0x0C;
constexpr uint32_t kDisplayModeFormat = 0x10;
constexpr uint32_t kDisplayModeSize = 0x14;

// D3DPRESENT_PARAMETERS (the leading PC-compatible part).
constexpr uint32_t kPresentBackBufferWidth = 0x00;
constexpr uint32_t kPresentBackBufferHeight = 0x04;
constexpr uint32_t kPresentBackBufferFormat = 0x08;
constexpr uint32_t kPresentBackBufferCount = 0x0C;
constexpr uint32_t kPresentMultiSampleType = 0x10;
constexpr uint32_t kPresentSwapEffect = 0x14;
constexpr uint32_t kPresentDeviceWindow = 0x18;
constexpr uint32_t kPresentWindowed = 0x1C;
constexpr uint32_t kPresentEnableAutoDepthStencil = 0x20;
constexpr uint32_t kPresentAutoDepthStencilFormat = 0x24;
constexpr uint32_t kPresentFlags = 0x28;
constexpr uint32_t kPresentRefreshRate = 0x2C;
constexpr uint32_t kPresentPresentationInterval = 0x30;
constexpr uint32_t kPresentSize = 0x34;

// D3DCAPS8 as the XDK 4134 library copies it: 53 DWORDs.
constexpr uint32_t kCapsDwords = 53;
}  // namespace layout

struct DisplayMode
{
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;
    uint32_t format;
    uint32_t flags;
};

// Presentation parameters the title asked for, kept as plain values.
struct PresentParameters
{
    uint32_t back_buffer_width = 0;
    uint32_t back_buffer_height = 0;
    uint32_t back_buffer_format = 0;
    uint32_t back_buffer_count = 0;
    uint32_t multi_sample_type = 0;
    uint32_t swap_effect = 0;
    uint32_t enable_auto_depth_stencil = 0;
    uint32_t auto_depth_stencil_format = 0;
    uint32_t flags = 0;
    uint32_t refresh_rate = 0;
    uint32_t presentation_interval = 0;
};

struct D3dState
{
    std::mutex mutex;
    GuestAddress device = 0;  // the guest device object, 0 before CreateDevice
    PresentParameters present;
    bool display_created = false;
};

D3dState& State();
runtime::GuestMemory& Memory();
runtime::GuestHeap& Heap();

// The adapter's display modes for the AV configuration the kernel reports
// (d3d8_adapter.cpp).
const DisplayMode* AdapterModes(uint32_t* count);

void RegisterAdapterExports(runtime::HleRegistry& registry);
void RegisterCapsExports(runtime::HleRegistry& registry);
void RegisterDeviceExports(runtime::HleRegistry& registry);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_INTERNAL_H
