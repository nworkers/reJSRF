// rexbox_d3d8_probe: checks the D3D8 HLE's pure parts without a game or a GPU:
// texture unswizzling and format decoding, DXT blocks, FVF layouts, the
// fixed-function GLSL generator (docs/design/20261012-i023-d3d8-fixed-function-draw.md
// section 7), immediate-mode vertex packing
// (docs/design/20261015-i062-immediate-mode.md), and the vertex-program and register-combiner translations
// (docs/design/20261013-i037-vertex-shader-programs.md). The GL backend itself
// is verified by booting.
//
// Usage: rexbox_d3d8_probe        (exit code 0 when every check passes)

#include "d3d8_ff_shader.h"
#include "d3d8_fvf.h"
#include "d3d8_gamma.h"
#include "d3d8_immediate.h"
#include "d3d8_psh.h"
#include "d3d8_texture_format.h"
#include "d3d8_vsh.h"

#include <cmath>
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

float FloatAt(const std::vector<uint8_t>& bytes, size_t offset)
{
    float value = 0.0f;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}

void CheckImmediate()
{
    // The demo's dimming overlay: FVF 1C4, colors set once, a texture
    // coordinate per vertex, then D3DVSDE_VERTEX.
    ImmediateBatch batch;
    Expect(!batch.active(), "no batch before Begin");
    batch.Begin(5);
    Expect(batch.Set(vsde::kDiffuse, 0.0f, 0.0f, 0.0f, 0.498f), "diffuse register");
    Expect(batch.Set(vsde::kSpecular, 1.0f, 0.5f, 0.25f, 0.0f), "specular register");
    const float corners[4][2] = {{0, 0}, {640, 0}, {0, 480}, {640, 480}};
    for (uint32_t i = 0; i < 4; ++i)
    {
        batch.Set(vsde::kTexCoord0, static_cast<float>(i & 1), static_cast<float>(i >> 1), 0.0f, 1.0f);
        batch.Set(vsde::kVertex, corners[i][0], corners[i][1], 0.5f, 0.5f);
    }
    batch.End();
    Expect(!batch.active() && batch.primitive() == 5, "End keeps the primitive");
    ExpectEqual(static_cast<uint32_t>(batch.vertices().size()), 4, "four vertices completed");
    Expect(!batch.Set(16, 0, 0, 0, 0), "register 16 is out of range");
    batch.Set(vsde::kVertex, 0, 0, 0, 1);
    ExpectEqual(static_cast<uint32_t>(batch.vertices().size()), 4, "no vertex completed outside a batch");

    VertexLayout layout;
    DecodeFvf(0x1C4, &layout);
    const std::vector<uint8_t> bytes = PackFvfVertices(layout, batch.vertices());
    ExpectEqual(static_cast<uint32_t>(bytes.size()), 4 * 32, "FVF 1C4 packs 32 bytes a vertex");
    Expect(FloatAt(bytes, 32 * 3) == 640.0f && FloatAt(bytes, 32 * 3 + 4) == 480.0f &&
               FloatAt(bytes, 32 * 3 + 8) == 0.5f && FloatAt(bytes, 32 * 3 + 12) == 0.5f,
           "XYZRHW position of the last vertex");
    Expect(bytes[16] == 0 && bytes[17] == 0 && bytes[18] == 0 && bytes[19] == 127, "diffuse as D3DCOLOR B, G, R, A");
    Expect(bytes[20] == 64 && bytes[21] == 128 && bytes[22] == 255 && bytes[23] == 0, "specular as B, G, R, A");
    Expect(FloatAt(bytes, 32 * 1 + 24) == 1.0f && FloatAt(bytes, 32 * 1 + 28) == 0.0f &&
               FloatAt(bytes, 32 * 2 + 24) == 0.0f && FloatAt(bytes, 32 * 2 + 28) == 1.0f,
           "texture coordinates per vertex");

    // Registers keep their value into the next batch; Begin drops vertices.
    batch.Begin(4);
    ExpectEqual(static_cast<uint32_t>(batch.vertices().size()), 0, "Begin starts empty");
    batch.Set(vsde::kVertex, 1, 2, 3, 1);
    Expect(batch.vertices()[0].reg[vsde::kDiffuse][3] == 0.498f, "diffuse kept across batches");
    Expect(batch.vertices()[0].reg[vsde::kNormal][3] == 1.0f && batch.vertices()[0].reg[vsde::kNormal][0] == 0.0f,
           "an unset register is (0, 0, 0, 1)");

    // A program declaration: v0 float3, v3 D3DCOLOR, v9 normalized shorts.
    vsh::Declaration declaration;
    vsh::VertexElement position;
    position.reg = 0;
    position.kind = vsh::ElementKind::kFloat;
    position.components = 3;
    vsh::VertexElement color;
    color.reg = 3;
    color.kind = vsh::ElementKind::kD3dColor;
    color.components = 4;
    color.offset = 12;
    vsh::VertexElement tex;
    tex.reg = 9;
    tex.kind = vsh::ElementKind::kNormShort;
    tex.components = 2;
    tex.offset = 16;
    declaration.elements = {position, color, tex};
    ImmediateBatch program;
    program.Begin(5);
    program.Set(3, 1.0f, 0.0f, 0.0f, 1.0f);
    program.Set(9, 1.0f, -1.0f, 0.0f, 1.0f);
    program.Set(vsde::kVertex, 7.0f, 8.0f, 9.0f, 1.0f);
    const std::vector<uint8_t> packed = PackDeclarationVertices(declaration, 20, program.vertices());
    ExpectEqual(static_cast<uint32_t>(packed.size()), 20, "declaration packs its stride");
    Expect(FloatAt(packed, 0) == 7.0f && FloatAt(packed, 8) == 9.0f, "program position in v0");
    Expect(packed[12] == 0 && packed[14] == 255 && packed[15] == 255, "program D3DCOLOR input");
    int16_t shorts[2] = {};
    std::memcpy(shorts, packed.data() + 16, 4);
    Expect(shorts[0] == 32767 && shorts[1] == -32767, "program normalized shorts");
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

void CheckGammaRamp()
{
    GammaRamp ramp;
    Expect(ramp.IsIdentity(), "a new ramp is identity");
    uint8_t pixel[8] = {10, 20, 30, 40, 200, 100, 0, 255};
    ApplyGammaRamp(ramp, pixel, 2);
    Expect(pixel[0] == 10 && pixel[1] == 20 && pixel[2] == 30 && pixel[3] == 40, "identity keeps the pixel");

    // A half-brightness fade on red and blue, green inverted.
    for (size_t i = 0; i < GammaRamp::kEntries; ++i)
    {
        ramp.bytes[i] = static_cast<uint8_t>(i / 2);
        ramp.bytes[GammaRamp::kEntries + i] = static_cast<uint8_t>(255 - i);
        ramp.bytes[2 * GammaRamp::kEntries + i] = static_cast<uint8_t>(i / 2);
    }
    Expect(!ramp.IsIdentity(), "a changed ramp is not identity");
    ApplyGammaRamp(ramp, pixel, 2);
    ExpectTexel(pixel, 5, 235, 15, 40, "ramp applied to pixel 0, alpha kept");
    ExpectTexel(pixel + 4, 100, 155, 0, 255, "ramp applied to pixel 1, alpha kept");

    const auto texels = GammaRampTexels(ramp);
    ExpectTexel(texels.data() + 255 * 4, 127, 0, 127, 255, "ramp texel 255 = red, green, blue tables");
    Expect(GammaPresentFragmentShader(false).find("texelFetch(u_ramp") != std::string::npos,
           "present shader looks the ramp up");
    Expect(GammaPresentVertexShader(true).rfind("#version 300 es", 0) == 0, "GLES present shader");
}


// --- Vertex programs -----------------------------------------------------------

// Packs one NV2A instruction from fields, following the layout in d3d8_vsh.h
// independently of the decoder.
struct Encoder
{
    uint32_t w[4] = {0, 0, 0, 0};
    void Put(int dword, int low, int width, uint32_t value)
    {
        const uint32_t mask = ((1u << width) - 1u) << low;
        w[dword] = (w[dword] & ~mask) | ((value << low) & mask);
    }
    // swizzle as "xyzw" letters
    static uint32_t Swizzle(const char* s)
    {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
        {
            const uint32_t c = s[i] == 'x' ? 0 : s[i] == 'y' ? 1 : s[i] == 'z' ? 2 : 3;
            v = (v << 2) | c;
        }
        return v;
    }
    void A(uint32_t mux, uint32_t temp, const char* swz, bool neg = false)
    {
        Put(2, 26, 2, mux);
        Put(2, 28, 4, temp);
        Put(1, 0, 8, Swizzle(swz));
        Put(1, 8, 1, neg ? 1 : 0);
    }
    void B(uint32_t mux, uint32_t temp, const char* swz, bool neg = false)
    {
        Put(2, 11, 2, mux);
        Put(2, 13, 4, temp);
        Put(2, 17, 8, Swizzle(swz));
        Put(2, 25, 1, neg ? 1 : 0);
    }
    void C(uint32_t mux, uint32_t temp, const char* swz, bool neg = false)
    {
        Put(3, 28, 2, mux);
        Put(2, 0, 2, temp >> 2);
        Put(3, 30, 2, temp & 3);
        Put(2, 2, 8, Swizzle(swz));
        Put(2, 10, 1, neg ? 1 : 0);
    }
};

void CheckVertexPrograms()
{
    // dp4 R0.x, v0, c[107]
    Encoder dp4;
    dp4.Put(1, 21, 4, vsh::mac::kDp4);
    dp4.Put(1, 9, 4, 0);
    dp4.Put(1, 13, 8, 107);
    dp4.A(vsh::mux::kInput, 0, "xyzw");
    dp4.B(vsh::mux::kConstant, 0, "xyzw");
    dp4.Put(3, 24, 4, 8);   // MAC mask x
    dp4.Put(3, 20, 4, 0);   // R0
    const vsh::Instruction d = vsh::Decode(dp4.w);
    Expect(d.mac == vsh::mac::kDp4 && d.constant == 107 && d.a.mux == vsh::mux::kInput &&
               d.b.mux == vsh::mux::kConstant && d.mac_mask == 8 && d.temp == 0 && !d.final,
           "decode dp4 R0.x, v0, c[107]");
    Expect(vsh::Disassemble(d) == "dp4 R0.x, v0, c[107]", "disassemble dp4: " + vsh::Disassemble(d));

    // The runtime epilogue: mul oPos.xyz, R12, c[58] + rcc R1.x, R12.w; then
    // mad oPos.xyz, R12, R1.x, c[59] (final).
    Encoder mul;
    mul.Put(1, 21, 4, vsh::mac::kMul);
    mul.Put(1, 25, 3, vsh::ilu::kRcc);
    mul.Put(1, 13, 8, 58);
    mul.A(vsh::mux::kTemp, 12, "xyzw");
    mul.B(vsh::mux::kConstant, 0, "xyzw");
    mul.C(vsh::mux::kTemp, 12, "wwww");
    mul.Put(3, 16, 4, 8);   // ILU mask x
    mul.Put(3, 20, 4, 7);   // temp field: ignored by the paired ILU, which writes R1
    mul.Put(3, 12, 4, 0xE); // out mask xyz
    mul.Put(3, 11, 1, 1);   // to o[]
    mul.Put(3, 3, 8, vsh::out::kPosition);
    const vsh::Instruction m = vsh::Decode(mul.w);
    Expect(m.c.temp == 12 && m.c.mux == vsh::mux::kTemp, "C temp splits across dwords 2 and 3");
    Expect(vsh::Disassemble(m) == "mul oPos.xyz, R12, c[58] + rcc R1.x, R12.wwww",
           "disassemble the paired epilogue: " + vsh::Disassemble(m));

    Encoder mad;
    mad.Put(1, 21, 4, vsh::mac::kMad);
    mad.Put(1, 13, 8, 59);
    mad.A(vsh::mux::kTemp, 12, "xyzw");
    mad.B(vsh::mux::kTemp, 1, "xxxx");
    mad.C(vsh::mux::kConstant, 0, "xyzw");
    mad.Put(3, 12, 4, 0xE);
    mad.Put(3, 11, 1, 1);
    mad.Put(3, 3, 8, vsh::out::kPosition);
    mad.Put(3, 0, 1, 1);

    // Relative addressing and an output colour.
    Encoder rel;
    rel.Put(1, 21, 4, vsh::mac::kMov);
    rel.Put(1, 13, 8, 10);
    rel.A(vsh::mux::kConstant, 0, "xyzw", true);
    rel.Put(3, 1, 1, 1);
    rel.Put(3, 12, 4, 0xF);
    rel.Put(3, 11, 1, 1);
    rel.Put(3, 3, 8, vsh::out::kDiffuse);

    std::vector<uint8_t> bytes(4 + 4 * 16);
    const uint32_t header = (4u << 16) | vsh::kTypeVertex;
    std::memcpy(bytes.data(), &header, 4);
    std::memcpy(bytes.data() + 4, dp4.w, 16);
    std::memcpy(bytes.data() + 20, rel.w, 16);
    std::memcpy(bytes.data() + 36, mul.w, 16);
    std::memcpy(bytes.data() + 52, mad.w, 16);
    vsh::Program program;
    std::string error;
    Expect(vsh::ReadProgram(bytes.data(), bytes.size(), &program, &error) && program.words.size() == 4 &&
               program.type == vsh::kTypeVertex,
           "read a four-instruction program");
    Expect(!vsh::ReadProgram(bytes.data(), 40, &program, &error), "a truncated program is refused");
    vsh::ReadProgram(bytes.data(), bytes.size(), &program, &error);

    uint32_t used = 0;
    const std::string glsl = vsh::TranslateToGlsl(program, 1u << 3, false, &used);
    Expect(used == 1u, "the program reads v0 only");
    Expect(Contains(glsl, "layout(location = 0) in vec4 v0;"), "v0 is attribute 0");
    Expect(Contains(glsl, "vec4 m = vec4(dot(A, B));") && Contains(glsl, "R0.x = m.x;"), "dp4 writes R0.x");
    Expect(Contains(glsl, "c[clamp(a0 + 10, 0, 191)]") && Contains(glsl, "oD0.xyzw = m.xyzw;"),
           "relative constant read and oD0 write");
    Expect(Contains(glsl, "R1.x = i.x;") && !Contains(glsl, "R7.x = i.x;"), "a paired ILU writes R1");
    Expect(Contains(glsl, "vec4(rcc(C.x))"), "rcc");
    Expect(Contains(glsl, "(oPos.xyz - offset) / scale * oPos.w"), "the epilogue is undone");
    Expect(vsh::TranslateToGlsl(program, 0, true, &used).rfind("#version 300 es", 0) == 0, "GLES variant");

    // The epilogue and its undoing round-trip through the viewport constants.
    float scale[4];
    float offset[4];
    vsh::ViewportConstants(16, 8, 640, 480, scale, offset);
    const float clip[4] = {0.25f, -0.5f, 0.75f, 2.0f};
    bool round_trip = true;
    for (int i = 0; i < 3; ++i)
    {
        const float screen = clip[i] * scale[i] * (1.0f / clip[3]) + offset[i];
        const float back = (screen - offset[i]) / scale[i] * clip[3];
        round_trip = round_trip && std::fabs(back - clip[i]) < 1e-4f;
    }
    Expect(round_trip, "screen-space epilogue round-trips to clip space");
    Expect(scale[1] < 0.0f && offset[0] == 336.0f && offset[1] == 248.0f, "viewport constants: y flipped, centred");

    // A declaration: stream 0 with FLOAT3, FLOAT1, PBYTE4, FLOAT3, FLOAT2, FLOAT2.
    const std::vector<uint32_t> tokens = {0x20000000, 0x40320000, 0x40120001, 0x40440002, 0x40320003,
                                          0x40220004, 0x40220005, 0xFFFFFFFF};
    vsh::Declaration declaration;
    Expect(vsh::ParseDeclaration(tokens, &declaration, &error) && declaration.elements.size() == 6,
           "parse a six-register declaration");
    Expect(declaration.elements[2].kind == vsh::ElementKind::kNormByte && declaration.elements[2].offset == 16 &&
               declaration.elements[3].offset == 20 && declaration.elements[5].offset == 40 &&
               declaration.stream_bytes[0] == 48 && declaration.streams == 1,
           "declaration offsets and vertex size");
    const std::vector<uint32_t> color = {0x20000001, 0x40400003, 0x50020000, 0x40120004, 0xFFFFFFFF};
    Expect(vsh::ParseDeclaration(color, &declaration, &error) && declaration.bgra_inputs == (1u << 3) &&
               declaration.elements[1].offset == 12 && declaration.streams == 2,
           "D3DCOLOR register, skip of two DWORDs, stream 1");
    Expect(!vsh::ParseDeclaration({0x20000000, 0x40160000, 0xFFFFFFFF}, &declaration, &error),
           "NORMPACKED3 is refused for now");
    Expect(!vsh::ParseDeclaration({0x20000000, 0x40320000}, &declaration, &error), "a declaration needs an end");
}

// --- Register combiners --------------------------------------------------------

void CheckPixelShaders()
{
    // Two stages: r0 = v0 * t0 (signed identity), r1 = c0 * t2; then
    // r0.rgb = lerp by r1.a (r1.a * t2 + (1 - r1.a) * r0), no final combiner.
    psh::Definition def = {};
    def[psh::word::kRgbInputs + 0] = 0xC4C80000;
    def[psh::word::kRgbOutputs + 0] = 0x000000C0;
    def[psh::word::kAlphaInputs + 0] = 0xD4D81010;
    def[psh::word::kAlphaOutputs + 0] = 0x000000C0;
    def[psh::word::kRgbInputs + 1] = 0x1DCACC3D;
    def[psh::word::kRgbOutputs + 1] = 0x00000C00;
    def[psh::word::kCombinerCount] = 0x00011102;
    def[psh::word::kTextureModes] = 0x00008421;
    def[psh::word::kC0Mapping] = 0xFFFFFF0F;  // stage 1's c0 is register 0
    def[psh::word::kC1Mapping] = 0xFFFFFFFF;
    def[psh::word::kFinalConstants] = 0x000001FF;
    def[psh::word::kConstant0 + 0] = 0x80FF0000;  // stage 0's own c0
    psh::Key key;
    key.definition = def;
    key.samplers[0] = SamplerKind::k2D;
    key.samplers[2] = SamplerKind::k2D;
    std::string unsupported;
    const std::string fs = psh::GenerateFragmentShader(key, false, &unsupported);
    Expect(unsupported.empty(), "a 2D-only shader is fully supported");
    Expect(Contains(fs, "vec4 r0 = vec4(0.0, 0.0, 0.0, t0.a);"), "r0.a starts as t0.a");
    Expect(Contains(fs, "cab = (v0.rgb) * (t0.rgb);") && Contains(fs, "r0.rgb = cab;"), "stage 0 r0 = v0 * t0");
    Expect(Contains(fs, "(vec3(1.0) - clamp(vec3(r1.a), 0.0, 1.0))"), "unsigned invert of r1.a");
    Expect(Contains(fs, "r0.rgb = csum;"), "stage 1 writes the sum to r0");
    Expect(Contains(fs, "vec4 current = clamp(r0, 0.0, 1.0);"), "no final combiner: r0 is the output");
    Expect(Contains(fs, "texture(u_tex0,") && !Contains(fs, "u_tex1;") && Contains(fs, "vec4 t1 = vec4(1.0);"),
           "stage 1 has no bound texture: white");

    float pixel_constants[psh::kPixelConstants][4] = {};
    pixel_constants[0][0] = 0.5f;
    pixel_constants[0][3] = 1.0f;
    float c0[psh::kStages][4];
    float c1[psh::kStages][4];
    float f0[4];
    float f1[4];
    psh::StageConstants(def, pixel_constants, c0, c1, f0, f1);
    Expect(c0[1][0] == 0.5f && c0[1][3] == 1.0f, "stage 1 c0 comes from SetPixelShaderConstant(0)");
    Expect(c0[0][0] == 1.0f && std::fabs(c0[0][3] - 128.0f / 255.0f) < 1e-6f, "stage 0 c0 is its own D3DCOLOR");
    Expect(c1[3][0] == 0.0f && f0[0] == 0.0f, "unmapped c1 and final constants come from the definition");

    // A final combiner: A = fog.a, B = r0, C = fog.rgb, D = 0; G = r0.a.
    psh::Definition fin = def;
    fin[psh::word::kFinalInputsAbcd] = 0x130C0300;
    fin[psh::word::kFinalInputsEfg] = 0x00001C00;
    key.definition = fin;
    const std::string ff = psh::GenerateFragmentShader(key, false, &unsupported);
    Expect(Contains(ff, "vec3 fa = clamp(vec3(fog.a), 0.0, 1.0);") && Contains(ff, "(vec3(1.0) - fa) *"),
           "final combiner lerp");
    Expect(Contains(ff, "clamp(r0.a, 0.0, 1.0));"), "final alpha G = r0.a");

    psh::Definition bump = def;
    bump[psh::word::kTextureModes] = 0x000000C1;  // stage 1: BUMPENVMAP
    key.definition = bump;
    key.samplers[1] = SamplerKind::k2D;
    psh::GenerateFragmentShader(key, false, &unsupported);
    Expect(Contains(unsupported, "texture mode 6"), "bump environment mapping is reported");
    Expect(psh::TextureMode(bump, 1) == psh::texmode::kBumpEnvMap, "texture mode of stage 1");
}

}  // namespace

int main()
{
    CheckGammaRamp();
    CheckSwizzle();
    CheckPixelFormats();
    CheckDxt();
    CheckFvf();
    CheckImmediate();
    CheckFixedFunctionShader();
    CheckVertexPrograms();
    CheckPixelShaders();
    std::printf("%d check(s), %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
