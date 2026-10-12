// Device capabilities. The Xbox library answers from a constant D3DCAPS8
// table (the GPU is always the NV2A); this is that table, measured with
// rejsrf_d3d8_check --dump and re-checked by its comparison mode.

#include "d3d8_internal.h"

#include <array>

namespace rexbox::hle::d3d8
{

namespace
{

// The 53 DWORDs the XDK 4134 library copies (D3DCAPS8 field order:
// DeviceType, AdapterOrdinal, Caps, Caps2, Caps3, PresentationIntervals,
// CursorCaps, DevCaps, PrimitiveMiscCaps, RasterCaps, ZCmpCaps, SrcBlendCaps,
// DestBlendCaps, AlphaCmpCaps, ShadeCaps, TextureCaps, TextureFilterCaps,
// CubeTextureFilterCaps, VolumeTextureFilterCaps, TextureAddressCaps,
// VolumeTextureAddressCaps, LineCaps, MaxTextureWidth, MaxTextureHeight,
// MaxVolumeExtent, MaxTextureRepeat, MaxTextureAspectRatio, MaxAnisotropy,
// MaxVertexW, GuardBandLeft/Top/Right/Bottom, ExtentsAdjust, StencilCaps,
// FVFCaps, TextureOpCaps, MaxTextureBlendStages, MaxSimultaneousTextures,
// VertexProcessingCaps, MaxActiveLights, MaxUserClipPlanes,
// MaxVertexBlendMatrices, MaxVertexBlendMatrixIndex, MaxPointSize,
// MaxPrimitiveCount, MaxVertexIndex, MaxStreams, MaxStreamStride,
// VertexShaderVersion, MaxVertexShaderConst, PixelShaderVersion,
// MaxPixelShaderValue).
constexpr std::array<uint32_t, layout::kCapsDwords> kCaps = {{
    0x00000001, 0x00000000, 0x00020000, 0x00000000, 0x00000000, 0x80000003, 0x00000000, 0x007BBEF0,
    0x00000CF2, 0x00377101, 0x000000FF, 0x00001FFF, 0x00001FFF, 0x000000FF, 0x00084208, 0x0007EC87,
    0x1F030700, 0x1F030700, 0x00000000, 0x00000017, 0x00000000, 0x0000001F, 0x00001000, 0x00001000,
    0x00000000, 0x00002000, 0x00000000, 0x00000002, 0x501502F9, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x000000FF, 0x00080004, 0x00FFFFFF, 0x00000008, 0x00000004, 0x000000BB,
    0x00000008, 0x00000000, 0x00000004, 0x00000000, 0x42800000, 0x0000FFFF, 0x0000FFFF, 0x00000010,
    0x000000FF, 0xFFFF0101, 0x00000060, 0xFFFF0101, 0x3F800000,
}};

void WriteCaps(GuestAddress out)
{
    runtime::GuestMemory& memory = Memory();
    for (uint32_t i = 0; i < layout::kCapsDwords; ++i)
    {
        memory.Write32(out + i * 4, kCaps[i]);
    }
}

// void D3DDevice_GetDeviceCaps(D3DCAPS8* pCaps)
// The library copies the table unconditionally and leaves eax as it was.
void D3DDevice_GetDeviceCaps()
{
    HleCall call;
    const GuestAddress out = call.Arg(0);
    if (out != 0)
    {
        WriteCaps(out);
    }
    call.Return(call.cpu().Get(runtime::Gpr::kEax), 1);
}

// HRESULT D3D_GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS8* pCaps)
void D3D_GetDeviceCaps()
{
    HleCall call;
    if (call.Arg(0) != 0)
    {
        call.Return(kD3dErrInvalidCall, 3);
        return;
    }
    if (call.Arg(1) != kDevTypeHal)
    {
        call.Return(kD3dErrInvalidDevice, 3);
        return;
    }
    const GuestAddress out = call.Arg(2);
    if (out != 0)
    {
        WriteCaps(out);
    }
    call.Return(kD3dOk, 3);
}

}  // namespace

void RegisterCapsExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "D3DDevice_GetDeviceCaps", &D3DDevice_GetDeviceCaps);
    registry.AddExport("d3d8", "D3D_GetDeviceCaps", &D3D_GetDeviceCaps);
}

}  // namespace rexbox::hle::d3d8
