#include "d3d8_resource.h"

#include "d3d8_gl_textures.h"
#include "d3d8_internal.h"

#include <algorithm>
#include <unordered_map>

namespace rexbox::hle::d3d8
{

namespace
{

constexpr uint32_t kFmtDxt1 = 0x0C;
constexpr uint32_t kFmtDxt3 = 0x0E;
constexpr uint32_t kFmtDxt5 = 0x0F;

std::unordered_map<GuestAddress, HostResource> g_resources;

uint32_t Log2Floor(uint32_t value)
{
    uint32_t result = 0;
    while (value > 1)
    {
        value >>= 1;
        ++result;
    }
    return result;
}

bool IsCompressed(uint32_t format)
{
    return format == kFmtDxt1 || format == kFmtDxt3 || format == kFmtDxt5;
}

// Destroys a resource whose last reference went away (D3D_DestroyResource).
// Device-owned surfaces stay: the device holds them for the frame.
void DestroyResource(GuestAddress resource)
{
    const auto it = g_resources.find(resource);
    if (it == g_resources.end())
    {
        return;
    }
    if (it->second.kind == ResourceKind::kBackBuffer || it->second.kind == ResourceKind::kDepthStencil)
    {
        // Keep one reference so the header stays valid for the device.
        Memory().Write32(resource + header::kCommon, Memory().Read32(resource + header::kCommon) + 1);
        return;
    }
    if (it->second.owns_memory)
    {
        const GuestAddress data = Memory().Read32(resource + header::kData);
        if (data != 0)
        {
            Heap().Free(data);
        }
    }
    if (it->second.owns_header)
    {
        Heap().Free(resource);
    }
    ForgetTexture(resource);
    g_resources.erase(it);
}

// ULONG D3DResource_AddRef(D3DResource* pThis)
void D3DResource_AddRef()
{
    HleCall call;
    call.Return(AddResourceRef(call.Arg(0)), 1);
}

// ULONG D3DResource_Release(D3DResource* pThis)
void D3DResource_Release()
{
    HleCall call;
    call.Return(ReleaseResourceRef(call.Arg(0)), 1);
}

// D3DRESOURCETYPE D3DResource_GetType(D3DResource* pThis)
void D3DResource_GetType()
{
    HleCall call;
    call.Return(HeaderResourceType(call.Arg(0)), 1);
}

// Xbox D3DSURFACE_DESC: {Format, Type, Usage, Size, MultiSampleType, Width, Height}.
void WriteSurfaceDesc(GuestAddress resource, uint32_t level, GuestAddress desc)
{
    runtime::GuestMemory& memory = Memory();
    const uint32_t format = HeaderFormat(resource);
    uint32_t usage = 0;
    if (level == 0)
    {
        const uint8_t flags = FormatCapabilityFlags(format);
        if ((flags & 0x80) != 0)
        {
            usage = kUsageRenderTarget;
        }
        else if ((flags & 0x40) != 0)
        {
            usage = kUsageDepthStencil;
        }
    }
    const LevelLayout layout = HeaderLevelLayout(resource, level);
    const HostResource* host = FindResource(resource);
    const uint32_t multisample =
        host != nullptr && host->kind == ResourceKind::kBackBuffer ? State().present.multi_sample_type : 0x0011;
    memory.Write32(desc + 0x00, format);
    memory.Write32(desc + 0x04, HeaderResourceType(resource));
    memory.Write32(desc + 0x08, usage);
    memory.Write32(desc + 0x0C, layout.size);
    memory.Write32(desc + 0x10, multisample);
    memory.Write32(desc + 0x14, layout.width);
    memory.Write32(desc + 0x18, layout.height);
}

// HRESULT D3DSurface_GetDesc(D3DSurface* pThis, D3DSURFACE_DESC* pDesc)
// The library leaves the level width in eax rather than an HRESULT; callers
// ignore it, and the HLE returns the same.
void D3DSurface_GetDesc()
{
    HleCall call;
    WriteSurfaceDesc(call.Arg(0), 0, call.Arg(1));
    call.Return(HeaderLevelLayout(call.Arg(0), 0).width, 2);
}

}  // namespace

uint32_t FormatBitsPerPixel(uint32_t format)
{
    return FormatCapabilityFlags(format) & 0x3C;
}

uint32_t HeaderFormat(GuestAddress resource)
{
    return Memory().Read8(resource + header::kFormat + 1);
}

uint32_t HeaderResourceType(GuestAddress resource)
{
    runtime::GuestMemory& memory = Memory();
    const uint32_t type = memory.Read32(resource + header::kCommon) & header::kTypeMask;
    switch (type)
    {
    case header::kTypeVertexBuffer:
        return kResourceTypeVertexBuffer;
    case header::kTypeIndexBuffer:
        return kResourceTypeIndexBuffer;
    case header::kTypePushBuffer:
        return kResourceTypePushBuffer;
    case header::kTypePalette:
        return kResourceTypePalette;
    case header::kTypeTexture:
    {
        const uint32_t format = memory.Read32(resource + header::kFormat);
        if ((format & header::kFormatCubeMap) != 0)
        {
            return kResourceTypeCubeTexture;
        }
        return (format & 0xF0) > 0x20 ? kResourceTypeVolumeTexture : kResourceTypeTexture;
    }
    case header::kTypeSurface:
    {
        const uint32_t format = memory.Read32(resource + header::kFormat);
        return (format & 0xF0) > 0x20 ? kResourceTypeVolume : kResourceTypeSurface;
    }
    default:
        return kResourceTypeFixup;
    }
}

LevelLayout HeaderLevelLayout(GuestAddress resource, uint32_t level)
{
    runtime::GuestMemory& memory = Memory();
    const uint32_t format_field = memory.Read32(resource + header::kFormat);
    const uint32_t size_field = memory.Read32(resource + header::kSize);
    const uint32_t format = (format_field >> header::kFormatFormatShift) & 0xFF;
    LevelLayout layout;
    if (size_field != 0)
    {
        layout.width = (size_field & header::kSizeWidthMask) + 1;
        layout.height = ((size_field >> header::kSizeHeightShift) & header::kSizeHeightMask) + 1;
        layout.depth = 1;
        layout.pitch = ((size_field >> header::kSizePitchShift) + 1) << 6;
        layout.size = layout.height * layout.pitch;
        return layout;
    }
    const int32_t log2_width = static_cast<int32_t>((format_field >> header::kFormatLog2WidthShift) & 0xF) -
                               static_cast<int32_t>(level);
    const int32_t log2_height = static_cast<int32_t>((format_field >> header::kFormatLog2HeightShift) & 0xF) -
                                static_cast<int32_t>(level);
    const int32_t log2_depth = static_cast<int32_t>(format_field >> header::kFormatLog2DepthShift) -
                               static_cast<int32_t>(level);
    const uint32_t bpp = FormatBitsPerPixel(format);
    layout.depth = 1u << std::max(log2_depth, 0);
    // Compressed formats are addressed in 4x4 blocks, so a level is at least 4 wide and high.
    const int32_t minimum = IsCompressed(format) ? 2 : 0;
    layout.width = 1u << std::max(log2_width, minimum);
    layout.height = 1u << std::max(log2_height, minimum);
    if (format == kFmtDxt1)
    {
        layout.pitch = layout.width * 2;
    }
    else if (IsCompressed(format))
    {
        layout.pitch = layout.width * 4;
    }
    else
    {
        layout.pitch = (layout.width * bpp) >> 3;
    }
    layout.size = (layout.height * layout.width * bpp) >> 3;
    return layout;
}

uint32_t LinearPitch(uint32_t width, uint32_t format)
{
    return (((width * FormatBitsPerPixel(format)) >> 3) + 0x3F) & ~0x3Fu;
}

void EncodeLinear2D(uint32_t width, uint32_t height, uint32_t format, uint32_t pitch, uint32_t levels,
                    uint32_t* format_field, uint32_t* size_field)
{
    uint32_t value = 0;  // log2 depth, width and height are 0 for a linear resource
    value = (value << 4) | (levels & 0xF);
    value = (value << 8) | (format & 0xFF);
    value = (value << 4) | 2;  // 2D
    value = (value << 4) | 2;  // the value XGSetSurfaceHeader writes in the DMA nibble
    value |= header::kFormatBorderColor;
    *format_field = value;
    *size_field = (((pitch >> 6) - 1) << header::kSizePitchShift) |
                  (((height - 1) & header::kSizeHeightMask) << header::kSizeHeightShift) |
                  ((width - 1) & header::kSizeWidthMask);
}

GuestAddress CreateDeviceSurface(ResourceKind kind, uint32_t width, uint32_t height, uint32_t format)
{
    runtime::GuestMemory& memory = Memory();
    const uint32_t pitch = LinearPitch(width, format);
    const uint32_t data_size = pitch * height;
    const auto header_block = Heap().AllocateSmall(header::kSurfaceSize);
    const auto data_block = Heap().Allocate(data_size, runtime::GuestHeap::kPageSize, false);
    if (!header_block || !data_block)
    {
        return 0;
    }
    memory.Fill(*header_block, 0, header::kSurfaceSize);
    memory.Fill(*data_block, 0, data_size);
    uint32_t format_field = 0;
    uint32_t size_field = 0;
    EncodeLinear2D(width, height, format, pitch, 1, &format_field, &size_field);
    memory.Write32(*header_block + header::kCommon, 1 | header::kTypeSurface | header::kD3dCreated);
    memory.Write32(*header_block + header::kData, *data_block);
    memory.Write32(*header_block + header::kLock, 0);
    memory.Write32(*header_block + header::kFormat, format_field);
    memory.Write32(*header_block + header::kSize, size_field);
    memory.Write32(*header_block + header::kParent, 0);

    HostResource host;
    host.kind = kind;
    host.width = width;
    host.height = height;
    host.format = format;
    host.pitch = pitch;
    host.data_size = data_size;
    host.owns_memory = true;
    host.owns_header = true;
    g_resources[*header_block] = host;
    return *header_block;
}

HostResource* FindResource(GuestAddress resource)
{
    const auto it = g_resources.find(resource);
    return it == g_resources.end() ? nullptr : &it->second;
}

HostResource* AdoptResource(GuestAddress resource, ResourceKind kind)
{
    HostResource& host = g_resources[resource];
    host.kind = kind;
    const uint32_t type = Memory().Read32(resource + header::kCommon) & header::kTypeMask;
    if (type == header::kTypeTexture || type == header::kTypeSurface)
    {
        const LevelLayout layout = HeaderLevelLayout(resource, 0);
        host.width = layout.width;
        host.height = layout.height;
        host.pitch = layout.pitch;
        host.format = HeaderFormat(resource);
        host.levels = (Memory().Read32(resource + header::kFormat) >> header::kFormatMipLevelsShift) & 0xF;
        host.data_size = layout.size;
    }
    return &host;
}

uint32_t AddResourceRef(GuestAddress resource)
{
    runtime::GuestMemory& memory = Memory();
    const uint32_t common = memory.Read32(resource + header::kCommon);
    if ((common & header::kRefCountMask) == 0 && (common & header::kTypeMask) == header::kTypeSurface)
    {
        const GuestAddress parent = memory.Read32(resource + header::kParent);
        if (parent != 0)
        {
            AddResourceRef(parent);
        }
    }
    const uint32_t updated = common + 1;
    memory.Write32(resource + header::kCommon, updated);
    return updated & header::kRefCountMask;
}

uint32_t ReleaseResourceRef(GuestAddress resource)
{
    runtime::GuestMemory& memory = Memory();
    const uint32_t common = memory.Read32(resource + header::kCommon);
    if ((common & header::kRefCountMask) == 1)
    {
        if ((common & header::kTypeMask) == header::kTypeSurface)
        {
            const GuestAddress parent = memory.Read32(resource + header::kParent);
            if (parent != 0)
            {
                ReleaseResourceRef(parent);
            }
        }
        if ((memory.Read32(resource + header::kCommon) & header::kIntRefCountMask) == 0)
        {
            DestroyResource(resource);
            return 0;
        }
    }
    const uint32_t updated = memory.Read32(resource + header::kCommon) - 1;
    memory.Write32(resource + header::kCommon, updated);
    return updated & header::kRefCountMask;
}

void AddResourceBinding(GuestAddress resource)
{
    runtime::GuestMemory& memory = Memory();
    memory.Write32(resource + header::kCommon, memory.Read32(resource + header::kCommon) + header::kIntRefCountOne);
}

void ReleaseResourceBinding(GuestAddress resource, uint32_t lock_value)
{
    Memory().Write32(resource + header::kLock, lock_value);
    ReleaseResourceBinding(resource);
}

void ReleaseResourceBinding(GuestAddress resource)
{
    runtime::GuestMemory& memory = Memory();
    const uint32_t updated = memory.Read32(resource + header::kCommon) - header::kIntRefCountOne;
    memory.Write32(resource + header::kCommon, updated);
    if ((updated & (header::kRefCountMask | header::kIntRefCountMask)) == 0)
    {
        DestroyResource(resource);
    }
}

void RegisterResourceExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "D3DResource_AddRef", &D3DResource_AddRef);
    registry.AddExport("d3d8", "D3DResource_Release", &D3DResource_Release);
    registry.AddExport("d3d8", "D3DResource_GetType", &D3DResource_GetType);
    registry.AddExport("d3d8", "D3DSurface_GetDesc", &D3DSurface_GetDesc);
}

}  // namespace rexbox::hle::d3d8
