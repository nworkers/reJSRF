// The Direct3D object level: adapter, display modes and format support.
//
// The tables here reproduce what the XDK 4134 D3D8 library answers for the
// AV configuration the kernel HLE reports (NTSC-M, composite, no HD modes).
// They were measured with rejsrf_d3d8_check --dump and are re-checked by its
// comparison mode (docs/analysis/d3d8-adapter.md).

#include "d3d8_internal.h"

#include <array>

namespace rexbox::hle::d3d8
{

namespace
{

// Flags of the mode table, as the library reports them (meaning inferred;
// docs/analysis/d3d8-adapter.md).
constexpr uint32_t kModeFlags480 = 0x00000020;
constexpr uint32_t kModeFlags240 = 0x000000A0;
constexpr uint32_t kModeFlags480Alt = 0x00000120;
constexpr uint32_t kModeFlags240Alt = 0x000001A0;

constexpr uint32_t kFmtLinX8R8G8B8 = 0x1E;
constexpr uint32_t kFmtLinR5G6B5 = 0x11;
constexpr uint32_t kFmtLinX1R5G5B5 = 0x1C;
constexpr uint32_t kFmtLinA8R8G8B8 = 0x12;

// The 24 modes of a composite NTSC-M pack: six {width, height, flags}
// combinations, each in the four linear back-buffer formats, all 60 Hz.
constexpr std::array<DisplayMode, 24> kModes = {{
    {640, 480, 60, kFmtLinX8R8G8B8, kModeFlags480},   {640, 480, 60, kFmtLinR5G6B5, kModeFlags480},
    {640, 480, 60, kFmtLinX1R5G5B5, kModeFlags480},   {640, 480, 60, kFmtLinA8R8G8B8, kModeFlags480},
    {720, 480, 60, kFmtLinX8R8G8B8, kModeFlags480},   {720, 480, 60, kFmtLinR5G6B5, kModeFlags480},
    {720, 480, 60, kFmtLinX1R5G5B5, kModeFlags480},   {720, 480, 60, kFmtLinA8R8G8B8, kModeFlags480},
    {640, 240, 60, kFmtLinX8R8G8B8, kModeFlags240},   {640, 240, 60, kFmtLinR5G6B5, kModeFlags240},
    {640, 240, 60, kFmtLinX1R5G5B5, kModeFlags240},   {640, 240, 60, kFmtLinA8R8G8B8, kModeFlags240},
    {720, 240, 60, kFmtLinX8R8G8B8, kModeFlags240},   {720, 240, 60, kFmtLinR5G6B5, kModeFlags240},
    {720, 240, 60, kFmtLinX1R5G5B5, kModeFlags240},   {720, 240, 60, kFmtLinA8R8G8B8, kModeFlags240},
    {640, 480, 60, kFmtLinX8R8G8B8, kModeFlags480Alt}, {640, 480, 60, kFmtLinR5G6B5, kModeFlags480Alt},
    {640, 480, 60, kFmtLinX1R5G5B5, kModeFlags480Alt}, {640, 480, 60, kFmtLinA8R8G8B8, kModeFlags480Alt},
    {640, 240, 60, kFmtLinX8R8G8B8, kModeFlags240Alt}, {640, 240, 60, kFmtLinR5G6B5, kModeFlags240Alt},
    {640, 240, 60, kFmtLinX1R5G5B5, kModeFlags240Alt}, {640, 240, 60, kFmtLinA8R8G8B8, kModeFlags240Alt},
}};

// The library's per-D3DFORMAT capability byte, indexed by the D3DFORMAT
// value. Bit 7: color render target (also required of the adapter format);
// bit 6: depth/stencil; bits 2-5: a plain resource format. The entries past
// the last real format (0x41) are the bytes that follow the table in the
// library and are kept so the answers match exactly.
constexpr std::array<uint8_t, 256> kFormatFlags = {{
    0x09, 0x09, 0x11, 0x91, 0x11, 0x91, 0xA1, 0xA1, 0x00, 0x00, 0x00, 0x09, 0x04, 0x00, 0x08, 0x08,  // 00
    0x12, 0x92, 0xA2, 0x8A, 0x00, 0x00, 0x12, 0x92, 0x00, 0x09, 0x11, 0x0A, 0x92, 0x12, 0xA2, 0x0A,  // 10
    0x12, 0x00, 0x00, 0x00, 0x10, 0x10, 0x00, 0x11, 0x11, 0x11, 0x61, 0x61, 0x51, 0x51, 0x62, 0x62,  // 20
    0x52, 0x52, 0x11, 0x21, 0x00, 0x12, 0x00, 0x12, 0x11, 0x11, 0x21, 0x21, 0x21, 0x12, 0x12, 0x22,  // 30
    0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,  // 40
    0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,  // 50
    0x00, 0x08, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00,  // 60
    0x00, 0x10, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00,  // 70
    0x00, 0x20, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,  // 80
    0x00, 0x40, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00,  // 90
    0x00, 0x80, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00,  // A0
    0x00, 0x12, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,  // B0
    0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,  // C0
    0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0xC2, 0x15, 0x3F,  // D0
    0x00, 0x00, 0x80, 0x3F, 0x4C, 0x37, 0xA9, 0x3F, 0x48, 0xE1, 0xCA, 0x3F, 0x00, 0x00, 0x01, 0x00,  // E0
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00,  // F0
}};

constexpr uint8_t kFormatRenderTarget = 0x80;
constexpr uint8_t kFormatDepthStencil = 0x40;
constexpr uint8_t kFormatResource = 0x3C;

uint8_t FormatFlags(uint32_t format)
{
    return format < kFormatFlags.size() ? kFormatFlags[format] : 0;
}

uint32_t CheckAdapterAndDevice(const HleCall& call)
{
    if (call.Arg(0) != 0)
    {
        return kD3dErrInvalidCall;
    }
    if (call.Arg(1) != kDevTypeHal)
    {
        return kD3dErrInvalidDevice;
    }
    return kD3dOk;
}

// UINT Direct3D_GetAdapterCount(<one unused DWORD>)
// The library's function pops one argument (ret 4), so the HLE does too.
void Direct3D_GetAdapterCount()
{
    HleCall call;
    call.Return(1, 1);
}

// UINT D3D_GetAdapterModeCount(UINT Adapter)
// The library ignores the adapter and pops one argument.
void D3D_GetAdapterModeCount()
{
    HleCall call;
    uint32_t count = 0;
    AdapterModes(&count);
    call.Return(count, 1);
}

// HRESULT D3D_EnumAdapterModes(UINT Adapter, UINT Mode, D3DDISPLAYMODE* pMode)
// The library ignores the adapter and writes through pMode unchecked; the
// HLE skips the write for a null pointer and answers the same.
void D3D_EnumAdapterModes()
{
    HleCall call;
    uint32_t count = 0;
    const DisplayMode* modes = AdapterModes(&count);
    const uint32_t index = call.Arg(1);
    const GuestAddress out = call.Arg(2);
    if (index >= count)
    {
        call.Return(kD3dErrInvalidCall, 3);
        return;
    }
    if (out != 0)
    {
        runtime::GuestMemory& memory = Memory();
        memory.Write32(out + layout::kDisplayModeWidth, modes[index].width);
        memory.Write32(out + layout::kDisplayModeHeight, modes[index].height);
        memory.Write32(out + layout::kDisplayModeRefreshRate, modes[index].refresh_rate);
        memory.Write32(out + layout::kDisplayModeFlags, modes[index].flags);
        memory.Write32(out + layout::kDisplayModeFormat, modes[index].format);
    }
    call.Return(kD3dOk, 3);
}

// HRESULT D3D_CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType,
//     D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
// The resource type plays no part; the usage picks which bit of the format's
// capability byte must be set.
void D3D_CheckDeviceFormat()
{
    HleCall call;
    const uint32_t verdict = CheckAdapterAndDevice(call);
    if (verdict != kD3dOk)
    {
        call.Return(verdict, 6);
        return;
    }
    const uint32_t usage = call.Arg(3);
    const uint8_t check_flags = FormatFlags(call.Arg(5));
    bool available = (FormatFlags(call.Arg(2)) & kFormatRenderTarget) != 0;
    if (available)
    {
        if ((usage & kUsageDepthStencil) != 0)
        {
            available = (check_flags & kFormatDepthStencil) != 0;
        }
        else if ((usage & kUsageRenderTarget) != 0)
        {
            available = (check_flags & kFormatRenderTarget) != 0;
        }
        else
        {
            available = (check_flags & kFormatResource) != 0;
        }
    }
    call.Return(available ? kD3dOk : kD3dErrNotAvailable, 6);
}

// HRESULT Direct3D_CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType,
//     D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType)
// The library indexes the capability table with the *Windowed* argument
// (not the surface format) and requires its render-target bit, then with the
// multisample type and requires the depth/stencil bit there. Every real call
// therefore answers "not available"; the HLE reproduces that exactly.
void Direct3D_CheckDeviceMultiSampleType()
{
    HleCall call;
    const uint32_t verdict = CheckAdapterAndDevice(call);
    if (verdict != kD3dOk)
    {
        call.Return(verdict, 5);
        return;
    }
    const bool available = (FormatFlags(call.Arg(3)) & kFormatRenderTarget) != 0 &&
                           (FormatFlags(call.Arg(4)) & kFormatDepthStencil) != 0;
    call.Return(available ? kD3dOk : kD3dErrNotAvailable, 5);
}

}  // namespace

uint8_t FormatCapabilityFlags(uint32_t format)
{
    return FormatFlags(format);
}

const DisplayMode* AdapterModes(uint32_t* count)
{
    *count = static_cast<uint32_t>(kModes.size());
    return kModes.data();
}

void RegisterAdapterExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "Direct3D_GetAdapterCount", &Direct3D_GetAdapterCount);
    registry.AddExport("d3d8", "D3D_GetAdapterModeCount", &D3D_GetAdapterModeCount);
    registry.AddExport("d3d8", "D3D_EnumAdapterModes", &D3D_EnumAdapterModes);
    registry.AddExport("d3d8", "D3D_CheckDeviceFormat", &D3D_CheckDeviceFormat);
    registry.AddExport("d3d8", "Direct3D_CheckDeviceMultiSampleType", &Direct3D_CheckDeviceMultiSampleType);
}

}  // namespace rexbox::hle::d3d8
