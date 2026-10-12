// Resource objects: the guest-visible D3DResource header in its Xbox layout
// and the host-side table that pairs each header with host state.
//
// The game reads header fields and builds headers itself (XGRAPHC runs as
// lifted code, D3DResource_Register hands them to D3D), so the layout and the
// bit fields below are the original's (docs/kb/xbox-d3d8-api.md section 6,
// confirmed against the lifted library in rejsrf_d3d8_check).

#ifndef REXBOX_HLE_D3D8_D3D8_RESOURCE_H
#define REXBOX_HLE_D3D8_D3D8_RESOURCE_H

#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"

#include <cstdint>

namespace rexbox::hle::d3d8
{

using runtime::GuestAddress;

namespace header
{
// D3DResource / D3DPixelContainer / D3DSurface, in bytes.
constexpr uint32_t kCommon = 0x00;
constexpr uint32_t kData = 0x04;
constexpr uint32_t kLock = 0x08;
constexpr uint32_t kFormat = 0x0C;  // pixel containers
constexpr uint32_t kSize = 0x10;
constexpr uint32_t kParent = 0x14;  // surfaces: the owning texture, or 0
constexpr uint32_t kSurfaceSize = 0x18;

// Common.
constexpr uint32_t kRefCountMask = 0x0000FFFF;
constexpr uint32_t kTypeMask = 0x00070000;
constexpr uint32_t kTypeVertexBuffer = 0x00000000;
constexpr uint32_t kTypeIndexBuffer = 0x00010000;
constexpr uint32_t kTypePushBuffer = 0x00020000;
constexpr uint32_t kTypePalette = 0x00030000;
constexpr uint32_t kTypeTexture = 0x00040000;
constexpr uint32_t kTypeSurface = 0x00050000;
constexpr uint32_t kTypeFixup = 0x00060000;
constexpr uint32_t kIntRefCountMask = 0x00780000;
constexpr uint32_t kD3dCreated = 0x01000000;

// Format (the XGRAPHC bit field).
constexpr uint32_t kFormatDmaMask = 0x00000003;  // XGSetSurfaceHeader writes 2 here
constexpr uint32_t kFormatCubeMap = 0x00000004;
constexpr uint32_t kFormatBorderColor = 0x00000008;
constexpr uint32_t kFormatDimensionShift = 4;   // 2 = 2D, 3 = 3D
constexpr uint32_t kFormatFormatShift = 8;      // D3DFORMAT
constexpr uint32_t kFormatMipLevelsShift = 16;
constexpr uint32_t kFormatLog2WidthShift = 20;
constexpr uint32_t kFormatLog2HeightShift = 24;
constexpr uint32_t kFormatLog2DepthShift = 28;

// Size (linear resources only; 0 for swizzled power-of-two ones).
constexpr uint32_t kSizeWidthMask = 0x00000FFF;    // width - 1
constexpr uint32_t kSizeHeightShift = 12;           // height - 1
constexpr uint32_t kSizeHeightMask = 0x00000FFF;
constexpr uint32_t kSizePitchShift = 24;            // pitch / 64 - 1
}  // namespace header

// D3DRESOURCETYPE.
constexpr uint32_t kResourceTypeSurface = 1;
constexpr uint32_t kResourceTypeVolume = 2;
constexpr uint32_t kResourceTypeTexture = 3;
constexpr uint32_t kResourceTypeVolumeTexture = 4;
constexpr uint32_t kResourceTypeCubeTexture = 5;
constexpr uint32_t kResourceTypeVertexBuffer = 6;
constexpr uint32_t kResourceTypeIndexBuffer = 7;
constexpr uint32_t kResourceTypePushBuffer = 8;
constexpr uint32_t kResourceTypePalette = 9;
constexpr uint32_t kResourceTypeFixup = 10;

// Dimensions of one mip level of a pixel container, as the library derives
// them from the header (Get2DSurfaceDesc's helper).
struct LevelLayout
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t pitch = 0;
    uint32_t size = 0;
};

// Bits per pixel of a D3DFORMAT (the capability table's bits 2-5).
uint32_t FormatBitsPerPixel(uint32_t format);

// Reads header fields.
uint32_t HeaderFormat(GuestAddress resource);     // the D3DFORMAT byte
uint32_t HeaderResourceType(GuestAddress resource);  // D3DRESOURCETYPE
LevelLayout HeaderLevelLayout(GuestAddress resource, uint32_t level);

// Encodes a linear 2D surface or texture header's Format/Size pair as
// XGSetSurfaceHeader does (`levels` 1 for a surface).
void EncodeLinear2D(uint32_t width, uint32_t height, uint32_t format, uint32_t pitch, uint32_t levels,
                    uint32_t* format_field, uint32_t* size_field);

// Pitch of a linear surface the library would choose for width x format: the
// row bytes rounded up to 64.
uint32_t LinearPitch(uint32_t width, uint32_t format);

// What the HLE knows about a resource beyond its header.
enum class ResourceKind
{
    kBackBuffer,
    kDepthStencil,
    kGameSurface,     // registered by the game (D3DResource_Register)
    kImageSurface,    // D3DDevice_CreateImageSurface
    kLevelSurface,    // D3DTexture_GetSurfaceLevel (Parent is the texture)
    kTexture,
    kVolumeTexture,
    kVertexBuffer,
    kIndexBuffer,
};

struct HostResource
{
    ResourceKind kind = ResourceKind::kGameSurface;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint32_t pitch = 0;
    uint32_t data_size = 0;
    uint32_t levels = 1;
    bool owns_memory = false;  // header and data came from the HLE's heap
    bool owns_header = false;
    int back_buffer_index = 0;
};

// Byte offset of mip level `level` inside a texture's pixel memory, and the
// size of the whole chain, from the header's Format field.
uint32_t TextureLevelOffset(GuestAddress texture, uint32_t level);
uint32_t TextureChainSize(uint32_t format_field, uint32_t size_field);

// Creates a linear surface the device owns: a 24-byte header and its pixel
// memory from the guest heap, refcount 1, D3DCREATED. Returns 0 when memory
// is exhausted.
GuestAddress CreateDeviceSurface(ResourceKind kind, uint32_t width, uint32_t height, uint32_t format);

// Looks a header up; nullptr when the HLE does not know it.
HostResource* FindResource(GuestAddress resource);

// Registers a header the HLE did not create (decoded from its fields).
HostResource* AdoptResource(GuestAddress resource, ResourceKind kind);

// Adds `delta` to the header's reference count the way the library does
// (parents of surfaces follow) and returns the new count.
uint32_t AddResourceRef(GuestAddress resource);
uint32_t ReleaseResourceRef(GuestAddress resource);

void RegisterResourceExports(runtime::HleRegistry& registry);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_RESOURCE_H
