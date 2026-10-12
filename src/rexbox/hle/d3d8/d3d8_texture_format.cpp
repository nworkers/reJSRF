#include "d3d8_texture_format.h"

#include <algorithm>
#include <cstring>

namespace rexbox::hle::d3d8
{

namespace
{

uint32_t Log2(uint32_t value)
{
    uint32_t result = 0;
    while ((1u << result) < value)
    {
        ++result;
    }
    return result;
}

uint32_t Expand(uint32_t value, uint32_t bits)
{
    // Bit replication to 8 bits (5 -> 8: v << 3 | v >> 2, ...).
    uint32_t result = value << (8 - bits);
    for (uint32_t filled = bits; filled < 8; filled += bits)
    {
        result |= result >> filled;
    }
    return result & 0xFF;
}

void Store(uint8_t* out, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    out[0] = static_cast<uint8_t>(r);
    out[1] = static_cast<uint8_t>(g);
    out[2] = static_cast<uint8_t>(b);
    out[3] = static_cast<uint8_t>(a);
}

uint32_t Load(const uint8_t* source, uint32_t bytes)
{
    uint32_t value = 0;
    std::memcpy(&value, source, bytes);
    return value;
}

// One uncompressed texel to RGBA8.
void ConvertTexel(uint32_t format, const uint8_t* source, uint8_t* out)
{
    switch (format)
    {
    case fmt::kA8R8G8B8:
    case fmt::kLinA8R8G8B8:
    {
        const uint32_t v = Load(source, 4);
        Store(out, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, v >> 24);
        return;
    }
    case fmt::kX8R8G8B8:
    case fmt::kLinX8R8G8B8:
    {
        const uint32_t v = Load(source, 4);
        Store(out, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, 0xFF);
        return;
    }
    case fmt::kA8B8G8R8:
    case fmt::kLinA8B8G8R8:
    {
        const uint32_t v = Load(source, 4);
        Store(out, v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24);
        return;
    }
    case fmt::kB8G8R8A8:
    case fmt::kLinB8G8R8A8:
    {
        const uint32_t v = Load(source, 4);
        Store(out, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24, v & 0xFF);
        return;
    }
    case fmt::kR8G8B8A8:
    case fmt::kLinR8G8B8A8:
    {
        const uint32_t v = Load(source, 4);
        Store(out, v >> 24, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
        return;
    }
    case fmt::kR5G6B5:
    case fmt::kLinR5G6B5:
    {
        const uint32_t v = Load(source, 2);
        Store(out, Expand(v >> 11, 5), Expand((v >> 5) & 0x3F, 6), Expand(v & 0x1F, 5), 0xFF);
        return;
    }
    case fmt::kA1R5G5B5:
    case fmt::kLinA1R5G5B5:
    {
        const uint32_t v = Load(source, 2);
        Store(out, Expand((v >> 10) & 0x1F, 5), Expand((v >> 5) & 0x1F, 5), Expand(v & 0x1F, 5),
              (v & 0x8000) != 0 ? 0xFF : 0);
        return;
    }
    case fmt::kX1R5G5B5:
    case fmt::kLinX1R5G5B5:
    {
        const uint32_t v = Load(source, 2);
        Store(out, Expand((v >> 10) & 0x1F, 5), Expand((v >> 5) & 0x1F, 5), Expand(v & 0x1F, 5), 0xFF);
        return;
    }
    case fmt::kA4R4G4B4:
    case fmt::kLinA4R4G4B4:
    {
        const uint32_t v = Load(source, 2);
        Store(out, Expand((v >> 8) & 0xF, 4), Expand((v >> 4) & 0xF, 4), Expand(v & 0xF, 4), Expand(v >> 12, 4));
        return;
    }
    case fmt::kL8:
    case fmt::kLinL8:
        Store(out, source[0], source[0], source[0], 0xFF);
        return;
    case fmt::kAL8:
    case fmt::kLinAL8:
        // One byte read as both luminance and alpha (inferred).
        Store(out, source[0], source[0], source[0], source[0]);
        return;
    case fmt::kA8:
    case fmt::kLinA8:
        // Color channels read as 0, as with PC D3D8's A8 (inferred).
        Store(out, 0, 0, 0, source[0]);
        return;
    case fmt::kA8L8:
    case fmt::kLinA8L8:
    {
        const uint32_t v = Load(source, 2);
        Store(out, v & 0xFF, v & 0xFF, v & 0xFF, v >> 8);
        return;
    }
    default:
        Store(out, 0xFF, 0x00, 0xFF, 0xFF);  // unsupported: magenta
        return;
    }
}

void Color565(uint32_t value, uint32_t* r, uint32_t* g, uint32_t* b)
{
    *r = Expand(value >> 11, 5);
    *g = Expand((value >> 5) & 0x3F, 6);
    *b = Expand(value & 0x1F, 5);
}

}  // namespace

bool DescribeTextureFormat(uint32_t format, TextureFormatInfo* info)
{
    TextureFormatInfo result;
    switch (format)
    {
    case fmt::kA8R8G8B8:
    case fmt::kX8R8G8B8:
    case fmt::kA8B8G8R8:
    case fmt::kB8G8R8A8:
    case fmt::kR8G8B8A8:
        result.bits_per_pixel = 32;
        break;
    case fmt::kLinA8R8G8B8:
    case fmt::kLinX8R8G8B8:
    case fmt::kLinA8B8G8R8:
    case fmt::kLinB8G8R8A8:
    case fmt::kLinR8G8B8A8:
        result.bits_per_pixel = 32;
        result.layout = PixelLayout::kLinear;
        break;
    case fmt::kR5G6B5:
    case fmt::kA1R5G5B5:
    case fmt::kX1R5G5B5:
    case fmt::kA4R4G4B4:
    case fmt::kA8L8:
        result.bits_per_pixel = 16;
        break;
    case fmt::kLinR5G6B5:
    case fmt::kLinA1R5G5B5:
    case fmt::kLinX1R5G5B5:
    case fmt::kLinA4R4G4B4:
    case fmt::kLinA8L8:
        result.bits_per_pixel = 16;
        result.layout = PixelLayout::kLinear;
        break;
    case fmt::kL8:
    case fmt::kAL8:
    case fmt::kA8:
        result.bits_per_pixel = 8;
        break;
    case fmt::kLinL8:
    case fmt::kLinAL8:
    case fmt::kLinA8:
        result.bits_per_pixel = 8;
        result.layout = PixelLayout::kLinear;
        break;
    case fmt::kDxt1:
        result.bits_per_pixel = 4;
        result.layout = PixelLayout::kCompressed;
        break;
    case fmt::kDxt3:
    case fmt::kDxt5:
        result.bits_per_pixel = 8;
        result.layout = PixelLayout::kCompressed;
        break;
    case fmt::kP8:
        result.bits_per_pixel = 8;
        result.palette = true;
        break;
    default:
        return false;
    }
    if (info != nullptr)
    {
        *info = result;
    }
    return true;
}

uint32_t SwizzleOffset(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height, uint32_t depth,
                       uint32_t bytes_per_pixel)
{
    const uint32_t log_w = Log2(width);
    const uint32_t log_h = Log2(height);
    const uint32_t log_d = Log2(depth);
    uint32_t offset = 0;
    uint32_t bit = 0;
    for (uint32_t i = 0; i < std::max({log_w, log_h, log_d}); ++i)
    {
        if (i < log_w)
        {
            offset |= ((x >> i) & 1u) << bit++;
        }
        if (i < log_h)
        {
            offset |= ((y >> i) & 1u) << bit++;
        }
        if (i < log_d)
        {
            offset |= ((z >> i) & 1u) << bit++;
        }
    }
    return offset * bytes_per_pixel;
}

uint32_t LevelBytes(uint32_t format, uint32_t width, uint32_t height, uint32_t depth, uint32_t pitch)
{
    TextureFormatInfo info;
    if (!DescribeTextureFormat(format, &info))
    {
        return 0;
    }
    if (info.layout == PixelLayout::kCompressed)
    {
        const uint32_t block_bytes = info.bits_per_pixel * 2;  // 16 texels
        return ((width + 3) / 4) * ((height + 3) / 4) * block_bytes * depth;
    }
    const uint32_t row = info.layout == PixelLayout::kLinear && pitch != 0 ? pitch : width * info.bits_per_pixel / 8;
    return row * height * depth;
}

void DecodeDxtBlock(uint32_t format, const uint8_t* block, uint8_t rgba[64])
{
    const uint8_t* color_block = format == fmt::kDxt1 ? block : block + 8;
    const uint32_t c0 = Load(color_block, 2);
    const uint32_t c1 = Load(color_block + 2, 2);
    const uint32_t indices = Load(color_block + 4, 4);
    uint32_t palette[4][4];
    Color565(c0, &palette[0][0], &palette[0][1], &palette[0][2]);
    Color565(c1, &palette[1][0], &palette[1][1], &palette[1][2]);
    palette[0][3] = palette[1][3] = 0xFF;
    const bool four_colors = format != fmt::kDxt1 || c0 > c1;
    for (int channel = 0; channel < 3; ++channel)
    {
        if (four_colors)
        {
            palette[2][channel] = (2 * palette[0][channel] + palette[1][channel]) / 3;
            palette[3][channel] = (palette[0][channel] + 2 * palette[1][channel]) / 3;
        }
        else
        {
            palette[2][channel] = (palette[0][channel] + palette[1][channel]) / 2;
            palette[3][channel] = 0;
        }
    }
    palette[2][3] = 0xFF;
    palette[3][3] = four_colors ? 0xFF : 0;
    for (int i = 0; i < 16; ++i)
    {
        const uint32_t* color = palette[(indices >> (2 * i)) & 3];
        Store(rgba + i * 4, color[0], color[1], color[2], color[3]);
    }
    if (format == fmt::kDxt3)
    {
        for (int i = 0; i < 16; ++i)
        {
            const uint32_t nibble = (block[i / 2] >> ((i & 1) * 4)) & 0xF;
            rgba[i * 4 + 3] = static_cast<uint8_t>(nibble * 17);
        }
    }
    else if (format == fmt::kDxt5)
    {
        const uint32_t a0 = block[0];
        const uint32_t a1 = block[1];
        uint32_t alpha[8] = {a0, a1};
        if (a0 > a1)
        {
            for (uint32_t i = 1; i < 7; ++i)
            {
                alpha[i + 1] = ((7 - i) * a0 + i * a1) / 7;
            }
        }
        else
        {
            for (uint32_t i = 1; i < 5; ++i)
            {
                alpha[i + 1] = ((5 - i) * a0 + i * a1) / 5;
            }
            alpha[6] = 0;
            alpha[7] = 0xFF;
        }
        uint64_t bits = 0;
        for (int i = 0; i < 6; ++i)
        {
            bits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
        }
        for (int i = 0; i < 16; ++i)
        {
            rgba[i * 4 + 3] = static_cast<uint8_t>(alpha[(bits >> (3 * i)) & 7]);
        }
    }
}

bool DecodeTextureLevel(uint32_t format, const uint8_t* source, uint32_t width, uint32_t height, uint32_t depth,
                        uint32_t pitch, std::vector<uint8_t>* rgba)
{
    TextureFormatInfo info;
    if (!DescribeTextureFormat(format, &info) || info.palette)
    {
        return false;
    }
    rgba->assign(static_cast<size_t>(width) * height * depth * 4, 0);
    uint8_t* out = rgba->data();
    if (info.layout == PixelLayout::kCompressed)
    {
        const uint32_t block_bytes = info.bits_per_pixel * 2;
        const uint32_t blocks_x = (width + 3) / 4;
        const uint32_t blocks_y = (height + 3) / 4;
        uint8_t texels[64];
        for (uint32_t z = 0; z < depth; ++z)
        {
            for (uint32_t by = 0; by < blocks_y; ++by)
            {
                for (uint32_t bx = 0; bx < blocks_x; ++bx)
                {
                    DecodeDxtBlock(format, source + ((z * blocks_y + by) * blocks_x + bx) * block_bytes, texels);
                    for (uint32_t ty = 0; ty < 4 && by * 4 + ty < height; ++ty)
                    {
                        for (uint32_t tx = 0; tx < 4 && bx * 4 + tx < width; ++tx)
                        {
                            const size_t pixel = (static_cast<size_t>(z) * height + by * 4 + ty) * width + bx * 4 + tx;
                            std::memcpy(out + pixel * 4, texels + (ty * 4 + tx) * 4, 4);
                        }
                    }
                }
            }
        }
        return true;
    }
    const uint32_t bytes = info.bits_per_pixel / 8;
    const uint32_t row = pitch != 0 ? pitch : width * bytes;
    for (uint32_t z = 0; z < depth; ++z)
    {
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint32_t offset = info.layout == PixelLayout::kLinear
                                            ? (z * height + y) * row + x * bytes
                                            : SwizzleOffset(x, y, z, width, height, depth, bytes);
                const size_t pixel = (static_cast<size_t>(z) * height + y) * width + x;
                ConvertTexel(format, source + offset, out + pixel * 4);
            }
        }
    }
    return true;
}

}  // namespace rexbox::hle::d3d8
