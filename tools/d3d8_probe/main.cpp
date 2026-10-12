// rexbox_d3d8_probe: checks the D3D8 HLE's pure parts without a game or a GPU:
// texture unswizzling and format decoding, DXT blocks, FVF layouts and the
// fixed-function GLSL generator (docs/design/20261012-i023-d3d8-fixed-function-draw.md
// section 7). The GL backend itself is verified by booting.
//
// Usage: rexbox_d3d8_probe        (exit code 0 when every check passes)

#include "d3d8_ff_shader.h"
#include "d3d8_fvf.h"
#include "d3d8_texture_format.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

using namespace rexbox::hle::d3d8;

int g_checks = 0;
int g_failures = 0;

void Expect(bool condition, const std::string& what)
{
    ++g_checks;
    if (!condition)
    {
        ++g_failures;
        std::printf("FAIL %s\n", what.c_str());
    }
}

void ExpectEqual(uint32_t actual, uint32_t expected, const std::string& what)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), " (got %X, expected %X)", actual, expected);
    Expect(actual == expected, what + buffer);
}

void ExpectTexel(const uint8_t* texel, uint32_t r, uint32_t g, uint32_t b, uint32_t a, const std::string& what)
{
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), " (got %u,%u,%u,%u, expected %u,%u,%u,%u)", texel[0], texel[1], texel[2],
                  texel[3], r, g, b, a);
    Expect(texel[0] == r && texel[1] == g && texel[2] == b && texel[3] == a, what + buffer);
}

void CheckSwizzle()
{
    // Square: x and y bits alternate, x lowest.
    ExpectEqual(SwizzleOffset(1, 0, 0, 4, 4, 1, 1), 1, "swizzle 4x4 (1,0)");
    ExpectEqual(SwizzleOffset(0, 1, 0, 4, 4, 1, 1), 2, "swizzle 4x4 (0,1)");
    ExpectEqual(SwizzleOffset(2, 0, 0, 4, 4, 1, 1), 4, "swizzle 4x4 (2,0)");
    ExpectEqual(SwizzleOffset(0, 2, 0, 4, 4, 1, 1), 8, "swizzle 4x4 (0,2)");
    ExpectEqual(SwizzleOffset(3, 3, 0, 4, 4, 1, 4), 60, "swizzle 4x4 (3,3) x4 bytes");
    // Wide: once y runs out, x's remaining bits follow.
    ExpectEqual(SwizzleOffset(0, 1, 0, 8, 2, 1, 1), 2, "swizzle 8x2 (0,1)");
    ExpectEqual(SwizzleOffset(2, 0, 0, 8, 2, 1, 1), 4, "swizzle 8x2 (2,0)");
    ExpectEqual(SwizzleOffset(4, 0, 0, 8, 2, 1, 1), 8, "swizzle 8x2 (4,0)");
    ExpectEqual(SwizzleOffset(7, 1, 0, 8, 2, 1, 1), 15, "swizzle 8x2 (7,1)");
    // Tall.
    ExpectEqual(SwizzleOffset(1, 0, 0, 2, 8, 1, 1), 1, "swizzle 2x8 (1,0)");
    ExpectEqual(SwizzleOffset(0, 4, 0, 2, 8, 1, 1), 8, "swizzle 2x8 (0,4)");
    // Volume: x, y, z interleave.
    ExpectEqual(SwizzleOffset(0, 0, 1, 2, 2, 2, 1), 4, "swizzle 2x2x2 (0,0,1)");

    // Round trip: texel (x, y) of a swizzled 8x4 A8R8G8B8 image carries x in
    // red and y in green; decoding must restore raster order.
    const uint32_t width = 8;
    const uint32_t height = 4;
    std::vector<uint8_t> image(width * height * 4, 0);
    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const uint32_t value = 0xFF000000u | (x << 16) | (y << 8);
            std::memcpy(image.data() + SwizzleOffset(x, y, 0, width, height, 1, 4), &value, 4);
        }
    }
    std::vector<uint8_t> rgba;
    Expect(DecodeTextureLevel(fmt::kA8R8G8B8, image.data(), width, height, 1, 0, &rgba), "decode swizzled A8R8G8B8");
    bool ordered = rgba.size() == width * height * 4;
    for (uint32_t y = 0; ordered && y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const uint8_t* texel = rgba.data() + (y * width + x) * 4;
            ordered = ordered && texel[0] == x && texel[1] == y && texel[2] == 0 && texel[3] == 0xFF;
        }
    }
    Expect(ordered, "swizzled 8x4 decodes to raster order");
}

void CheckPixelFormats()
{
    std::vector<uint8_t> rgba;
    const uint16_t r565 = 0xF800;
    DecodeTextureLevel(fmt::kLinR5G6B5, reinterpret_cast<const uint8_t*>(&r565), 1, 1, 1, 2, &rgba);
    ExpectTexel(rgba.data(), 255, 0, 0, 255, "R5G6B5 red");
    const uint16_t g565 = 0x07E0;
    DecodeTextureLevel(fmt::kR5G6B5, reinterpret_cast<const uint8_t*>(&g565), 1, 1, 1, 0, &rgba);
    ExpectTexel(rgba.data(), 0, 255, 0, 255, "R5G6B5 green");
    const uint16_t a1 = 0x801F;
    DecodeTextureLevel(fmt::kA1R5G5B5, reinterpret_cast<const uint8_t*>(&a1), 1, 1, 1, 0, &rgba);
    ExpectTexel(rgba.data(), 0, 0, 255, 255, "A1R5G5B5 opaque blue");
    const uint16_t x1 = 0x7C00;
    DecodeTextureLevel(fmt::kX1R5G5B5, reinterpret_cast<const uint8_t*>(&x1), 1, 1, 1, 0, &rgba);
    ExpectTexel(rgba.data(), 255, 0, 0, 255, "X1R5G5B5 red ignores bit 15");
    const uint16_t a4 = 0x80F0;
    DecodeTextureLevel(fmt::kA4R4G4B4, reinterpret_cast<const uint8_t*>(&a4), 1, 1, 1, 0, &rgba);
    ExpectTexel(rgba.data(), 0, 255, 0, 136, "A4R4G4B4");
    const uint32_t argb = 0x80102030;
    DecodeTextureLevel(fmt::kLinA8R8G8B8, reinterpret_cast<const uint8_t*>(&argb), 1, 1, 1, 4, &rgba);
    ExpectTexel(rgba.data(), 0x10, 0x20, 0x30, 0x80, "LIN_A8R8G8B8");
    DecodeTextureLevel(fmt::kX8R8G8B8, reinterpret_cast<const uint8_t*>(&argb), 1, 1, 1, 0, &rgba);
    ExpectTexel(rgba.data(), 0x10, 0x20, 0x30, 0xFF, "X8R8G8B8 opaque");
    const uint8_t l8 = 0x7F;
    DecodeTextureLevel(fmt::kL8, &l8, 1, 1, 1, 0, &rgba);
    ExpectTexel(rgba.data(), 0x7F, 0x7F, 0x7F, 0xFF, "L8");
    const uint16_t a8l8 = 0x40C0;
    DecodeTextureLevel(fmt::kA8L8, reinterpret_cast<const uint8_t*>(&a8l8), 1, 1, 1, 0, &rgba);
    ExpectTexel(rgba.data(), 0xC0, 0xC0, 0xC0, 0x40, "A8L8");

    // Linear with a pitch larger than the row.
    const uint8_t pitched[8] = {1, 0xEE, 0xEE, 0xEE, 2, 0xEE, 0xEE, 0xEE};
    DecodeTextureLevel(fmt::kLinL8, pitched, 1, 2, 1, 4, &rgba);
    Expect(rgba.size() == 8 && rgba[0] == 1 && rgba[4] == 2, "LIN_L8 honors the pitch");

    ExpectEqual(LevelBytes(fmt::kDxt1, 512, 512, 1, 0), 512 * 512 / 2, "DXT1 512x512 bytes");
    ExpectEqual(LevelBytes(fmt::kDxt5, 2, 2, 1, 0), 16, "DXT5 2x2 is one block");
    ExpectEqual(LevelBytes(fmt::kA8R8G8B8, 16, 8, 1, 0), 512, "A8R8G8B8 16x8 bytes");
    ExpectEqual(LevelBytes(fmt::kLinR5G6B5, 10, 3, 1, 64), 192, "LIN_R5G6B5 uses the pitch");
    Expect(!DescribeTextureFormat(0x99, nullptr), "unknown format rejected");
}

void CheckDxt()
{
    uint8_t texels[64];
    // DXT1, four-color mode (c0 > c1): red to blue.
    const uint8_t four[8] = {0x00, 0xF8, 0x1F, 0x00, 0xE4, 0x00, 0x00, 0x00};  // indices 0,1,2,3 in the first row
    DecodeDxtBlock(fmt::kDxt1, four, texels);
    ExpectTexel(texels + 0, 255, 0, 0, 255, "DXT1 index 0");
    ExpectTexel(texels + 4, 0, 0, 255, 255, "DXT1 index 1");
    ExpectTexel(texels + 8, 170, 0, 85, 255, "DXT1 index 2 = 2/3 c0 + 1/3 c1");
    ExpectTexel(texels + 12, 85, 0, 170, 255, "DXT1 index 3 = 1/3 c0 + 2/3 c1");
    ExpectTexel(texels + 16, 255, 0, 0, 255, "DXT1 second row index 0");
    // DXT1, three-color mode (c0 <= c1): index 3 is transparent black.
    const uint8_t three[8] = {0x1F, 0x00, 0x00, 0xF8, 0xE4, 0x00, 0x00, 0x00};
    DecodeDxtBlock(fmt::kDxt1, three, texels);
    ExpectTexel(texels + 8, 127, 0, 127, 255, "DXT1 3-color index 2 = average");
    ExpectTexel(texels + 12, 0, 0, 0, 0, "DXT1 3-color index 3 transparent");
    // DXT3: explicit 4-bit alpha, color block always four-color.
    uint8_t dxt3[16] = {0x0F, 0xF0, 0, 0, 0, 0, 0, 0, 0x1F, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00};
    DecodeDxtBlock(fmt::kDxt3, dxt3, texels);
    ExpectTexel(texels + 0, 0, 0, 255, 255, "DXT3 texel 0 alpha 15");
    Expect(texels[7] == 0 && texels[11] == 0 && texels[15] == 255, "DXT3 alpha nibbles in order");
    // DXT5: 8 interpolated alphas between 255 and 0.
    uint8_t dxt5[16] = {255, 0, 0x88, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x00, 0x00, 0, 0, 0, 0};  // texel0 idx0, texel1 idx1, texel2 idx2
    DecodeDxtBlock(fmt::kDxt5, dxt5, texels);
    Expect(texels[3] == 255 && texels[7] == 0 && texels[11] == 218, "DXT5 alpha 255, 0, 6/7 * 255");
    ExpectTexel(texels + 0, 255, 0, 0, 255, "DXT5 color");

    // A whole 8x8 DXT1 image: block (1, 0) red, the rest black.
    std::vector<uint8_t> image(4 * 8, 0);
    image[8 + 1] = 0xF8;  // block 1: c0 = red, indices 0
    std::vector<uint8_t> rgba;
    Expect(DecodeTextureLevel(fmt::kDxt1, image.data(), 8, 8, 1, 0, &rgba), "decode DXT1 8x8");
    ExpectTexel(rgba.data() + (0 * 8 + 4) * 4, 255, 0, 0, 255, "DXT1 8x8 block 1 at x 4");
    ExpectTexel(rgba.data() + (3 * 8 + 3) * 4, 0, 0, 0, 255, "DXT1 8x8 block 0 stays black");
}

void CheckFvf()
{
    VertexLayout layout;
    Expect(DecodeFvf(0x1C4, &layout), "FVF 1C4 decodes");
    ExpectEqual(layout.stride, 32, "FVF 1C4 stride");
    Expect(layout.pretransformed && layout.position_components == 4, "FVF 1C4 is XYZRHW");
    ExpectEqual(static_cast<uint32_t>(layout.diffuse), 16, "FVF 1C4 diffuse offset");
    ExpectEqual(static_cast<uint32_t>(layout.specular), 20, "FVF 1C4 specular offset");
    ExpectEqual(layout.tex_coords, 1, "FVF 1C4 one texture set");
    ExpectEqual(static_cast<uint32_t>(layout.tex_offset[0]), 24, "FVF 1C4 texture offset");
    ExpectEqual(layout.tex_components[0], 2, "FVF 1C4 texture is 2D");

    Expect(DecodeFvf(0x152, &layout), "FVF 152 decodes");
    ExpectEqual(layout.stride, 36, "FVF XYZ|NORMAL|DIFFUSE|TEX1 stride");
    Expect(!layout.pretransformed && layout.normal == 12 && layout.diffuse == 24, "FVF 152 offsets");

    Expect(DecodeFvf(0x0010202 | 0x0000, &layout), "FVF with TEXCOORDSIZE decodes");
    Expect(DecodeFvf(0x00010202, &layout) && layout.tex_coords == 2 && layout.tex_components[0] == 3 &&
               layout.tex_components[1] == 2 && layout.stride == 12 + 12 + 8,
           "FVF TEXCOORDSIZE3(0) with two sets");
    Expect(DecodeFvf(0x00A, &layout) && layout.blend_weights == 3 && layout.stride == 24, "FVF XYZB3");
    Expect(!DecodeFvf(0x00D1F001, &layout), "a program handle is not an FVF");
    Expect(!DecodeFvf(0x040, &layout), "an FVF without a position is rejected");
}

bool Contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

void CheckFixedFunctionShader()
{
    FixedFunctionKey key;
    key.pretransformed = true;
    key.has_diffuse = true;
    key.has_specular = true;
    key.tex_coords = 1;
    key.tex_components[0] = 2;
    key.stages[0].color_op = top::kModulate;
    key.stages[0].color_arg[1] = ta::kTexture;
    key.stages[0].color_arg[2] = ta::kDiffuse;
    key.stages[0].alpha_op = top::kModulate;
    key.stages[0].alpha_arg[1] = ta::kTexture;
    key.stages[0].alpha_arg[2] = ta::kDiffuse;
    key.stages[0].sampler = SamplerKind::k2D;
    key.stages[1].color_op = top::kModulate;  // untextured stage: reads white
    const std::string vs = GenerateFixedFunctionVertexShader(key, false);
    const std::string fs = GenerateFixedFunctionFragmentShader(key, false);
    Expect(vs.rfind("#version 330 core", 0) == 0, "vertex shader is GLSL 330 core");
    Expect(Contains(vs, "1.0 / a_position.w"), "XYZRHW vertex shader divides by rhw");
    Expect(Contains(vs, "a_diffuse.bgra"), "D3DCOLOR swizzled to RGBA");
    Expect(Contains(fs, "texture(u_tex0, v_tex0.xy)"), "stage 0 samples its 2D texture");
    Expect(Contains(fs, "(t0).rgb * (v_diffuse).rgb"), "stage 0 modulates texture and diffuse");
    Expect(Contains(fs, "vec4 t1 = vec4(1.0)"), "unbound stage 1 reads white");
    Expect(!Contains(fs, "u_tex1"), "unbound stage declares no sampler");
    Expect(!Contains(fs, "discard"), "no alpha test without alpha_func");

    FixedFunctionKey tested = key;
    tested.alpha_func = 0x204;  // GREATER
    const std::string fs_tested = GenerateFixedFunctionFragmentShader(tested, true);
    Expect(fs_tested.rfind("#version 300 es", 0) == 0, "GLES variant");
    Expect(Contains(fs_tested, "alpha > u_alpha_ref") && Contains(fs_tested, "discard"), "alpha test GREATER");
    Expect(key.Serialize() != tested.Serialize(), "alpha test changes the key");

    FixedFunctionKey transformed = key;
    transformed.pretransformed = false;
    Expect(Contains(GenerateFixedFunctionVertexShader(transformed, false), "p.z * 2.0 - p.w"),
           "transformed path maps D3D depth to GL");

    // Stages after the first disabled one do not change the key.
    FixedFunctionKey a = key;
    FixedFunctionKey b = key;
    a.stages[1].color_op = top::kDisable;
    b.stages[1].color_op = top::kDisable;
    b.stages[2].color_op = top::kAdd;
    Expect(a.Serialize() == b.Serialize(), "stages after DISABLE are ignored");

    FixedFunctionKey temp = key;
    temp.stages[0].result_arg = ta::kTemp;
    temp.stages[1].color_arg[1] = ta::kTemp | ta::kComplement;
    const std::string fs_temp = GenerateFixedFunctionFragmentShader(temp, false);
    Expect(Contains(fs_temp, "temp = clamp(") && Contains(fs_temp, "(vec4(1.0) - temp)"),
           "TEMP result and complemented argument");
}

}  // namespace

int main()
{
    CheckSwizzle();
    CheckPixelFormats();
    CheckDxt();
    CheckFvf();
    CheckFixedFunctionShader();
    std::printf("%d check(s), %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
