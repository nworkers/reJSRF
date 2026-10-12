// Xbox texture formats to RGBA8 (pure functions, no GL; verified by
// rexbox_d3d8_probe). The NV2A stores swizzled textures in Morton order
// (x and y address bits interleaved, x lowest; once the smaller dimension
// runs out, the larger one's remaining bits follow), linear ones row by row
// with a pitch, and DXT blocks in 4x4 groups in raster order of blocks.
// Everything is decoded on the CPU so hosts without S3TC (GLES, WebGL)
// behave the same (docs/design/20261012-i023-d3d8-fixed-function-draw.md
// section 3.3).

#ifndef REXBOX_HLE_D3D8_D3D8_TEXTURE_FORMAT_H
#define REXBOX_HLE_D3D8_D3D8_TEXTURE_FORMAT_H

#include <cstdint>
#include <vector>

namespace rexbox::hle::d3d8
{

// Xbox D3DFORMAT values (NV2A color formats; docs/kb/xbox-d3d8-api.md section 8).
namespace fmt
{
constexpr uint32_t kL8 = 0x00;
constexpr uint32_t kAL8 = 0x01;
constexpr uint32_t kA1R5G5B5 = 0x02;
constexpr uint32_t kX1R5G5B5 = 0x03;
constexpr uint32_t kA4R4G4B4 = 0x04;
constexpr uint32_t kR5G6B5 = 0x05;
constexpr uint32_t kA8R8G8B8 = 0x06;
constexpr uint32_t kX8R8G8B8 = 0x07;
constexpr uint32_t kP8 = 0x0B;
constexpr uint32_t kDxt1 = 0x0C;
constexpr uint32_t kDxt3 = 0x0E;
constexpr uint32_t kDxt5 = 0x0F;
constexpr uint32_t kLinA1R5G5B5 = 0x10;
constexpr uint32_t kLinR5G6B5 = 0x11;
constexpr uint32_t kLinA8R8G8B8 = 0x12;
constexpr uint32_t kLinL8 = 0x13;
constexpr uint32_t kA8 = 0x19;
constexpr uint32_t kA8L8 = 0x1A;
constexpr uint32_t kLinAL8 = 0x1B;
constexpr uint32_t kLinX1R5G5B5 = 0x1C;
constexpr uint32_t kLinA4R4G4B4 = 0x1D;
constexpr uint32_t kLinX8R8G8B8 = 0x1E;
constexpr uint32_t kLinA8 = 0x1F;
constexpr uint32_t kLinA8L8 = 0x20;
constexpr uint32_t kA8B8G8R8 = 0x3A;
constexpr uint32_t kB8G8R8A8 = 0x3B;
constexpr uint32_t kR8G8B8A8 = 0x3C;
constexpr uint32_t kLinA8B8G8R8 = 0x3F;
constexpr uint32_t kLinB8G8R8A8 = 0x40;
constexpr uint32_t kLinR8G8B8A8 = 0x41;
}  // namespace fmt

enum class PixelLayout
{
    kSwizzled,
    kLinear,
    kCompressed,  // DXT: 4x4 blocks
};

struct TextureFormatInfo
{
    uint32_t bits_per_pixel = 0;  // per pixel; for DXT, per pixel of a block (4 or 8)
    PixelLayout layout = PixelLayout::kSwizzled;
    bool palette = false;  // P8: needs a palette to decode
};

// False for a format this module does not decode.
bool DescribeTextureFormat(uint32_t format, TextureFormatInfo* info);

// Byte offset of texel (x, y, z) in a swizzled image of the given size (all
// powers of two). Depth 1 for 2D.
uint32_t SwizzleOffset(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height, uint32_t depth,
                       uint32_t bytes_per_pixel);

// Bytes one level of `width` x `height` (x `depth`) occupies in guest memory.
// `pitch` is used for linear formats (0 means tightly packed).
uint32_t LevelBytes(uint32_t format, uint32_t width, uint32_t height, uint32_t depth, uint32_t pitch);

// Decodes one level into RGBA8 (4 bytes per texel, R first, top row first,
// slices one after another). `source` holds LevelBytes() bytes. Returns
// false for an unsupported format.
bool DecodeTextureLevel(uint32_t format, const uint8_t* source, uint32_t width, uint32_t height, uint32_t depth,
                        uint32_t pitch, std::vector<uint8_t>* rgba);

// One 4x4 DXT block (8 bytes for DXT1, 16 for DXT3/DXT5) into 16 RGBA8 texels.
void DecodeDxtBlock(uint32_t format, const uint8_t* block, uint8_t rgba[64]);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_TEXTURE_FORMAT_H
