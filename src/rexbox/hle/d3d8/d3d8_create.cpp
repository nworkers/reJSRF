// Resource creation, registration and locking: textures, surfaces, vertex
// and index buffers. Every object is a guest header in the Xbox layout plus
// guest pixel/vertex memory from the heap (d3d8_resource.h); the drawing
// batch uploads the memory to GL when it is used.

#include "d3d8_internal.h"
#include "d3d8_resource.h"

#include <algorithm>

namespace rexbox::hle::d3d8
{

namespace
{

constexpr uint32_t kFmtLinearFirst = 0x10;  // the LIN_ formats the title uses sit in 0x10..0x41
constexpr uint32_t kResourceHeaderSize = 0x0C;
constexpr uint32_t kPixelContainerSize = 0x14;

bool IsLinearFormat(uint32_t format)
{
    // Linear formats of the XDK 4134 library (D3DFMT_LIN_*), from the format table.
    switch (format)
    {
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F: case 0x20: case 0x2E:
    case 0x2F: case 0x30: case 0x31: case 0x35: case 0x36: case 0x37: case 0x3D: case 0x3E:
    case 0x3F: case 0x40: case 0x41:
        return true;
    default:
        return false;
    }
}

uint32_t Log2Ceil(uint32_t value)
{
    uint32_t result = 0;
    while ((1u << result) < value)
    {
        ++result;
    }
    return result;
}

// A header of `header_bytes` from the heap, zeroed, with `data_bytes` of
// pixel or vertex memory (0 for none). Returns 0 when memory is exhausted.
GuestAddress AllocateObject(uint32_t header_bytes, uint32_t data_bytes, GuestAddress* data)
{
    const auto header = Heap().Allocate(header_bytes, runtime::GuestHeap::kPageSize, false);
    if (!header)
    {
        return 0;
    }
    Memory().Fill(*header, 0, header_bytes);
    *data = 0;
    if (data_bytes != 0)
    {
        const auto block = Heap().Allocate(data_bytes, runtime::GuestHeap::kPageSize, false);
        if (!block)
        {
            Heap().Free(*header);
            return 0;
        }
        Memory().Fill(*block, 0, data_bytes);
        *data = *block;
    }
    return *header;
}

HostResource* Track(GuestAddress header, ResourceKind kind, uint32_t width, uint32_t height, uint32_t format,
                    uint32_t pitch, uint32_t data_size, uint32_t levels)
{
    HostResource* host = AdoptResource(header, kind);
    host->width = width;
    host->height = height;
    host->format = format;
    host->pitch = pitch;
    host->data_size = data_size;
    host->levels = levels;
    host->owns_memory = true;
    host->owns_header = true;
    return host;
}

// Encodes a texture header for CreateTexture: linear formats keep a pitch in
// Size, swizzled ones a power-of-two chain in Format.
void EncodeTexture(uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, uint32_t format, bool cube,
                   uint32_t* format_field, uint32_t* size_field, uint32_t* pitch)
{
    if (IsLinearFormat(format))
    {
        *pitch = LinearPitch(width, format);
        EncodeLinear2D(width, height, format, *pitch, levels, format_field, size_field);
        return;
    }
    uint32_t value = Log2Ceil(depth);
    value = (value << 4) | Log2Ceil(height);
    value = (value << 4) | Log2Ceil(width);
    value = (value << 4) | (levels & 0xF);
    value = (value << 8) | (format & 0xFF);
    value = (value << 4) | (depth > 1 ? 3u : 2u);
    value = (value << 4) | 1u;  // DMA nibble of a swizzled texture (inferred)
    value |= header::kFormatBorderColor;
    if (cube)
    {
        value |= header::kFormatCubeMap;
    }
    *format_field = value;
    *size_field = 0;
    *pitch = (width * FormatBitsPerPixel(format)) >> 3;
}

uint32_t FullMipCount(uint32_t width, uint32_t height, uint32_t depth)
{
    return Log2Ceil(std::max({width, height, depth})) + 1;
}

// HRESULT D3DDevice_CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage,
//     D3DFORMAT Format, D3DPOOL Pool, D3DTexture** ppTexture)
void D3DDevice_CreateTexture()
{
    HleCall call;
    const uint32_t width = call.Arg(0);
    const uint32_t height = call.Arg(1);
    uint32_t levels = call.Arg(2);
    const uint32_t format = call.Arg(4);
    const GuestAddress out = call.Arg(6);
    if (out == 0 || width == 0 || height == 0)
    {
        call.Return(kD3dErrInvalidCall, 7);
        return;
    }
    if (levels == 0)
    {
        levels = IsLinearFormat(format) ? 1 : FullMipCount(width, height, 1);
    }
    uint32_t format_field = 0;
    uint32_t size_field = 0;
    uint32_t pitch = 0;
    EncodeTexture(width, height, 1, levels, format, false, &format_field, &size_field, &pitch);
    const uint32_t data_size = TextureChainSize(format_field, size_field);
    GuestAddress data = 0;
    const GuestAddress texture = AllocateObject(kPixelContainerSize, data_size, &data);
    if (texture == 0)
    {
        call.Return(kD3dErrNotAvailable, 7);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write32(texture + header::kCommon, 1 | header::kTypeTexture | header::kD3dCreated);
    memory.Write32(texture + header::kData, data);
    memory.Write32(texture + header::kFormat, format_field);
    memory.Write32(texture + header::kSize, size_field);
    Track(texture, ResourceKind::kTexture, width, height, format, pitch, data_size, levels);
    memory.Write32(out, texture);
    call.Return(kD3dOk, 7);
}

// HRESULT D3DDevice_CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels,
//     DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, D3DVolumeTexture** ppVolumeTexture)
void D3DDevice_CreateVolumeTexture()
{
    HleCall call;
    const uint32_t width = call.Arg(0);
    const uint32_t height = call.Arg(1);
    const uint32_t depth = call.Arg(2);
    uint32_t levels = call.Arg(3);
    const uint32_t format = call.Arg(5);
    const GuestAddress out = call.Arg(7);
    if (out == 0 || width == 0 || height == 0 || depth == 0)
    {
        call.Return(kD3dErrInvalidCall, 8);
        return;
    }
    if (levels == 0)
    {
        levels = FullMipCount(width, height, depth);
    }
    uint32_t format_field = 0;
    uint32_t size_field = 0;
    uint32_t pitch = 0;
    EncodeTexture(width, height, depth, levels, format, false, &format_field, &size_field, &pitch);
    const uint32_t data_size = TextureChainSize(format_field, size_field);
    GuestAddress data = 0;
    const GuestAddress texture = AllocateObject(kPixelContainerSize, data_size, &data);
    if (texture == 0)
    {
        call.Return(kD3dErrNotAvailable, 8);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write32(texture + header::kCommon, 1 | header::kTypeTexture | header::kD3dCreated);
    memory.Write32(texture + header::kData, data);
    memory.Write32(texture + header::kFormat, format_field);
    memory.Write32(texture + header::kSize, size_field);
    Track(texture, ResourceKind::kVolumeTexture, width, height, format, pitch, data_size, levels);
    memory.Write32(out, texture);
    call.Return(kD3dOk, 8);
}

// HRESULT D3DDevice_CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DSurface** ppSurface)
void D3DDevice_CreateImageSurface()
{
    HleCall call;
    const GuestAddress out = call.Arg(3);
    const GuestAddress surface = CreateDeviceSurface(ResourceKind::kImageSurface, call.Arg(0), call.Arg(1), call.Arg(2));
    if (out == 0 || surface == 0)
    {
        call.Return(surface == 0 ? kD3dErrNotAvailable : kD3dErrInvalidCall, 4);
        return;
    }
    Memory().Write32(out, surface);
    call.Return(kD3dOk, 4);
}

// A 12-byte buffer resource (vertex or index buffer) with `length` bytes of memory.
void CreateBuffer(HleCall& call, uint32_t type, ResourceKind kind, uint32_t length, GuestAddress out, int stack_args)
{
    if (out == 0 || length == 0)
    {
        call.Return(kD3dErrInvalidCall, stack_args);
        return;
    }
    GuestAddress data = 0;
    const GuestAddress buffer = AllocateObject(kResourceHeaderSize, length, &data);
    if (buffer == 0)
    {
        call.Return(kD3dErrNotAvailable, stack_args);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write32(buffer + header::kCommon, 1 | type | header::kD3dCreated);
    memory.Write32(buffer + header::kData, data);
    Track(buffer, kind, length, 1, 0, length, length, 1);
    memory.Write32(out, buffer);
    call.Return(kD3dOk, stack_args);
}

// HRESULT D3DDevice_CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, D3DVertexBuffer** ppVertexBuffer)
void D3DDevice_CreateVertexBuffer()
{
    HleCall call;
    CreateBuffer(call, header::kTypeVertexBuffer, ResourceKind::kVertexBuffer, call.Arg(0), call.Arg(4), 5);
}

// HRESULT D3DDevice_CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, D3DIndexBuffer** ppIndexBuffer)
void D3DDevice_CreateIndexBuffer()
{
    HleCall call;
    CreateBuffer(call, header::kTypeIndexBuffer, ResourceKind::kIndexBuffer, call.Arg(0), call.Arg(4), 5);
}

// HRESULT D3DResource_Register(D3DResource* pThis, void* pBase)
// The title built the header itself (XGSetSurfaceHeader and friends) with Data
// holding an offset; the library adds the base and, except for push buffers,
// masks it to a physical address (measured per type). Its eax is the new Data.
void D3DResource_Register()
{
    HleCall call;
    const GuestAddress resource = call.Arg(0);
    runtime::GuestMemory& memory = Memory();
    const uint32_t common = memory.Read32(resource + header::kCommon);
    uint32_t data = memory.Read32(resource + header::kData) + call.Arg(1);
    if ((common & header::kTypeMask) != header::kTypePushBuffer)
    {
        data &= 0x03FFFFFF;  // physical address; push buffers keep the full value
    }
    memory.Write32(resource + header::kData, data);
    switch (common & header::kTypeMask)
    {
    case header::kTypeTexture:
        AdoptResource(resource, ResourceKind::kTexture);
        break;
    case header::kTypeSurface:
        AdoptResource(resource, ResourceKind::kGameSurface);
        break;
    case header::kTypeVertexBuffer:
        AdoptResource(resource, ResourceKind::kVertexBuffer);
        break;
    case header::kTypeIndexBuffer:
        AdoptResource(resource, ResourceKind::kIndexBuffer);
        break;
    default:
        break;
    }
    call.Return(data, 2);
}

// HRESULT D3DTexture_GetSurfaceLevel(D3DTexture* pThis, UINT Level, D3DSurface** ppSurfaceLevel)
// A surface header over one level of the texture; Parent keeps the texture alive.
void D3DTexture_GetSurfaceLevel()
{
    HleCall call;
    const GuestAddress texture = call.Arg(0);
    const uint32_t level = call.Arg(1);
    const GuestAddress out = call.Arg(2);
    if (texture == 0 || out == 0)
    {
        call.Return(kD3dErrInvalidCall, 3);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    GuestAddress unused = 0;
    const GuestAddress surface = AllocateObject(header::kSurfaceSize, 0, &unused);
    if (surface == 0)
    {
        call.Return(kD3dErrNotAvailable, 3);
        return;
    }
    const uint32_t texture_format = memory.Read32(texture + header::kFormat);
    uint32_t format_field = texture_format & ~(0xFu << header::kFormatMipLevelsShift);
    format_field |= 1u << header::kFormatMipLevelsShift;
    if (memory.Read32(texture + header::kSize) == 0)
    {
        // Shrink the power-of-two sizes by the level.
        auto shrink = [&](uint32_t shift) {
            const uint32_t log2 = (format_field >> shift) & 0xF;
            format_field &= ~(0xFu << shift);
            format_field |= (log2 > level ? log2 - level : 0u) << shift;
        };
        shrink(header::kFormatLog2WidthShift);
        shrink(header::kFormatLog2HeightShift);
        shrink(header::kFormatLog2DepthShift);
    }
    memory.Write32(surface + header::kCommon, 1 | header::kTypeSurface | header::kD3dCreated);
    memory.Write32(surface + header::kData, memory.Read32(texture + header::kData) + TextureLevelOffset(texture, level));
    memory.Write32(surface + header::kFormat, format_field);
    memory.Write32(surface + header::kSize, memory.Read32(texture + header::kSize));
    memory.Write32(surface + header::kParent, texture);
    AddResourceRef(texture);
    const LevelLayout layout = HeaderLevelLayout(surface, 0);
    HostResource* host = Track(surface, ResourceKind::kLevelSurface, layout.width, layout.height,
                               HeaderFormat(surface), layout.pitch, layout.size, 1);
    host->owns_memory = false;
    memory.Write32(out, surface);
    call.Return(kD3dOk, 3);
}

// HRESULT D3DTexture_GetLevelDesc(D3DTexture* pThis, UINT Level, D3DSURFACE_DESC* pDesc)
// (jsrf-recomp's inferred name; the lifted body is empty, so the convention is inferred.)
void D3DTexture_GetLevelDesc()
{
    HleCall call;
    const GuestAddress texture = call.Arg(0);
    const uint32_t level = call.Arg(1);
    const GuestAddress desc = call.Arg(2);
    if (texture != 0 && desc != 0)
    {
        runtime::GuestMemory& memory = Memory();
        const LevelLayout layout = HeaderLevelLayout(texture, level);
        memory.Write32(desc + 0x00, HeaderFormat(texture));
        memory.Write32(desc + 0x04, HeaderResourceType(texture));
        memory.Write32(desc + 0x08, 0);
        memory.Write32(desc + 0x0C, layout.size);
        memory.Write32(desc + 0x10, 0x0011);
        memory.Write32(desc + 0x14, layout.width);
        memory.Write32(desc + 0x18, layout.height);
    }
    call.Return(kD3dOk, 3);
}

// HRESULT D3DSurface_LockRect(D3DSurface* pThis, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags)
// Guest memory is directly addressable, so a lock is the pitch and a pointer.
void D3DSurface_LockRect()
{
    HleCall call;
    const GuestAddress surface = call.Arg(0);
    const GuestAddress locked = call.Arg(1);
    if (surface == 0 || locked == 0)
    {
        call.Return(kD3dErrInvalidCall, 4);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    const LevelLayout layout = HeaderLevelLayout(surface, 0);
    uint32_t offset = 0;
    const GuestAddress rect = call.Arg(2);
    if (rect != 0)
    {
        const uint32_t left = memory.Read32(rect + 0);
        const uint32_t top = memory.Read32(rect + 4);
        offset = top * layout.pitch + ((left * FormatBitsPerPixel(HeaderFormat(surface))) >> 3);
    }
    memory.Write32(locked + 0, layout.pitch);
    memory.Write32(locked + 4, memory.Read32(surface + header::kData) + offset);
    call.Return(kD3dOk, 4);
}

// HRESULT D3DTexture_LockRect(D3DTexture* pThis, UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags)
void D3DTexture_LockRect()
{
    HleCall call;
    const GuestAddress texture = call.Arg(0);
    const uint32_t level = call.Arg(1);
    const GuestAddress locked = call.Arg(2);
    if (texture == 0 || locked == 0)
    {
        call.Return(kD3dErrInvalidCall, 5);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    const LevelLayout layout = HeaderLevelLayout(texture, level);
    uint32_t offset = TextureLevelOffset(texture, level);
    const GuestAddress rect = call.Arg(3);
    if (rect != 0)
    {
        const uint32_t left = memory.Read32(rect + 0);
        const uint32_t top = memory.Read32(rect + 4);
        offset += top * layout.pitch + ((left * FormatBitsPerPixel(HeaderFormat(texture))) >> 3);
    }
    memory.Write32(locked + 0, layout.pitch);
    memory.Write32(locked + 4, memory.Read32(texture + header::kData) + offset);
    call.Return(kD3dOk, 5);
}

// HRESULT D3DVolumeTexture_LockBox(D3DVolumeTexture* pThis, UINT Level, D3DLOCKED_BOX* pLockedBox, const D3DBOX* pBox, DWORD Flags)
void D3DVolumeTexture_LockBox()
{
    HleCall call;
    const GuestAddress texture = call.Arg(0);
    const uint32_t level = call.Arg(1);
    const GuestAddress locked = call.Arg(2);
    if (texture == 0 || locked == 0)
    {
        call.Return(kD3dErrInvalidCall, 5);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    const LevelLayout layout = HeaderLevelLayout(texture, level);
    memory.Write32(locked + 0, layout.pitch);
    memory.Write32(locked + 4, layout.pitch * layout.height);
    memory.Write32(locked + 8, memory.Read32(texture + header::kData) + TextureLevelOffset(texture, level));
    call.Return(kD3dOk, 5);
}

// HRESULT D3DVertexBuffer_Lock(D3DVertexBuffer* pThis, UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags)
void D3DVertexBuffer_Lock()
{
    HleCall call;
    const GuestAddress buffer = call.Arg(0);
    const GuestAddress out = call.Arg(3);
    if (buffer == 0 || out == 0)
    {
        call.Return(kD3dErrInvalidCall, 5);
        return;
    }
    Memory().Write32(out, Memory().Read32(buffer + header::kData) + call.Arg(1));
    call.Return(kD3dOk, 5);
}

// void* D3D_AllocContiguousMemory(SIZE_T dwSize, DWORD dwAllocAttributes)
// The library asks the kernel for contiguous memory (MmAllocateContiguousMemoryEx);
// every guest allocation comes from the one heap here.
void D3D_AllocContiguousMemory()
{
    HleCall call;
    const uint32_t size = call.Arg(0);
    const auto block = Heap().Allocate(size == 0 ? 1 : size, runtime::GuestHeap::kPageSize, false);
    if (block)
    {
        Memory().Fill(*block, 0, size);
    }
    call.Return(block ? *block : 0, 2);
}

// void D3D_FreeContiguousMemory(void* pMemory)
// (jsrf-recomp's inferred name; the lifted body is empty, so the convention is inferred.)
void D3D_FreeContiguousMemory()
{
    HleCall call;
    if (call.Arg(0) != 0)
    {
        Heap().Free(call.Arg(0));
    }
    call.Return(0, 1);
}

}  // namespace

uint32_t TextureLevelOffset(GuestAddress texture, uint32_t level)
{
    uint32_t offset = 0;
    for (uint32_t i = 0; i < level; ++i)
    {
        offset += HeaderLevelLayout(texture, i).size;
    }
    return offset;
}

uint32_t TextureChainSize(uint32_t format_field, uint32_t size_field)
{
    // Decode through a scratch header so the level sizes follow the library's rule.
    const auto scratch = Heap().Allocate(kPixelContainerSize, runtime::GuestHeap::kPageSize, true);
    if (!scratch)
    {
        return 0;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write32(*scratch + header::kFormat, format_field);
    memory.Write32(*scratch + header::kSize, size_field);
    const uint32_t levels = std::max(1u, (format_field >> header::kFormatMipLevelsShift) & 0xF);
    uint32_t total = 0;
    for (uint32_t level = 0; level < levels; ++level)
    {
        total += HeaderLevelLayout(*scratch, level).size;
    }
    Heap().Free(*scratch);
    return total;
}

void RegisterCreateExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "D3DDevice_CreateTexture", &D3DDevice_CreateTexture);
    registry.AddExport("d3d8", "D3DDevice_CreateVolumeTexture", &D3DDevice_CreateVolumeTexture);
    registry.AddExport("d3d8", "D3DDevice_CreateImageSurface", &D3DDevice_CreateImageSurface);
    registry.AddExport("d3d8", "D3DDevice_CreateVertexBuffer", &D3DDevice_CreateVertexBuffer);
    registry.AddExport("d3d8", "D3DDevice_CreateIndexBuffer", &D3DDevice_CreateIndexBuffer);
    registry.AddExport("d3d8", "D3DResource_Register", &D3DResource_Register);
    registry.AddExport("d3d8", "D3DTexture_GetSurfaceLevel", &D3DTexture_GetSurfaceLevel);
    registry.AddExport("d3d8", "D3DTexture_GetLevelDesc", &D3DTexture_GetLevelDesc);
    registry.AddExport("d3d8", "D3DSurface_LockRect", &D3DSurface_LockRect);
    registry.AddExport("d3d8", "D3DTexture_LockRect", &D3DTexture_LockRect);
    registry.AddExport("d3d8", "D3DVolumeTexture_LockBox", &D3DVolumeTexture_LockBox);
    registry.AddExport("d3d8", "D3DVertexBuffer_Lock", &D3DVertexBuffer_Lock);
    registry.AddExport("d3d8", "D3D_AllocContiguousMemory", &D3D_AllocContiguousMemory);
    registry.AddExport("d3d8", "D3D_FreeContiguousMemory", &D3D_FreeContiguousMemory);
}

}  // namespace rexbox::hle::d3d8
