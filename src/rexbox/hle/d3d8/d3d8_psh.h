// NV2A register-combiner pixel shaders (D3DPIXELSHADERDEF) as GLSL fragment
// shaders (pure, no GL; verified by rexbox_d3d8_probe).
//
// A definition is 60 DWORDs. Each of up to eight general combiner stages has
// an RGB and an alpha portion: four inputs A, B, C, D (a register, a channel
// and an input mapping per byte), the products A*B and C*D (or dot products),
// their sum or a mux, an output scale or bias, and up to three destination
// registers. A final combiner computes A*B + (1-A)*C + D and G. The model is
// the one OpenGL's NV_register_combiners specifies; the field encoding is the
// XDK's (docs/kb/xbox-d3d8-api.md, pixel shaders).

#ifndef REXBOX_HLE_D3D8_D3D8_PSH_H
#define REXBOX_HLE_D3D8_D3D8_PSH_H

#include "d3d8_ff_shader.h"

#include <array>
#include <cstdint>
#include <string>

namespace rexbox::hle::d3d8::psh
{

constexpr uint32_t kDefinitionWords = 60;
constexpr uint32_t kStages = 8;
constexpr uint32_t kPixelConstants = 16;  // SetPixelShaderConstant registers this HLE keeps

// DWORD indices into D3DPIXELSHADERDEF.
namespace word
{
constexpr uint32_t kAlphaInputs = 0;   // [8]
constexpr uint32_t kFinalInputsAbcd = 8;
constexpr uint32_t kFinalInputsEfg = 9;
constexpr uint32_t kConstant0 = 10;    // [8] D3DCOLOR per stage
constexpr uint32_t kConstant1 = 18;    // [8]
constexpr uint32_t kAlphaOutputs = 26; // [8]
constexpr uint32_t kRgbInputs = 34;    // [8]
constexpr uint32_t kCompareMode = 42;
constexpr uint32_t kFinalConstant0 = 43;
constexpr uint32_t kFinalConstant1 = 44;
constexpr uint32_t kRgbOutputs = 45;   // [8]
constexpr uint32_t kCombinerCount = 53;
constexpr uint32_t kTextureModes = 54;
constexpr uint32_t kDotMapping = 55;
constexpr uint32_t kInputTexture = 56;
constexpr uint32_t kC0Mapping = 57;
constexpr uint32_t kC1Mapping = 58;
constexpr uint32_t kFinalConstants = 59;
}  // namespace word

// Texture addressing modes (PS_TEXTUREMODES, 5 bits per stage).
namespace texmode
{
constexpr uint32_t kNone = 0, kProject2D = 1, kProject3D = 2, kCubeMap = 3, kPassThru = 4, kClipPlane = 5,
                   kBumpEnvMap = 6, kBumpEnvMapLum = 7;
}

using Definition = std::array<uint32_t, kDefinitionWords>;

struct Key
{
    Definition definition = {};
    SamplerKind samplers[4] = {SamplerKind::kNone, SamplerKind::kNone, SamplerKind::kNone, SamplerKind::kNone};
    bool unnormalized[4] = {};
    // A stage whose texture feeds the next stage's BUMPENVMAP: true when its
    // format is of the A8R8G8B8 family, where U, V and L sit in blue, green
    // and red (X8L8V8U8); otherwise U, V, L are red, green, blue (Q8W8V8U8).
    bool bump_source_argb[4] = {};
    uint32_t alpha_func = 0;  // 0: alpha test off; else 0x200 NEVER .. 0x207 ALWAYS

    std::string Serialize() const;
    bool operator==(const Key&) const = default;
    uint64_t Hash() const;
};

uint32_t TextureMode(const Definition& definition, uint32_t stage);

// The fragment shader: inputs v_diffuse, v_specular, v_tex0..3 (as the
// vertex-program translation writes them). Uniforms: u_tex0..3,
// u_tex_scale0..3 (vec2, 1/size for unnormalized textures), u_c0[8],
// u_c1[8] (per-stage constants), u_final_c0, u_final_c1, u_fog_color,
// u_alpha_ref (0..255). `unsupported` lists the features drawn approximately
// (texture modes other than 2D/3D/cube/pass-through, compare mode).
std::string GenerateFragmentShader(const Key& key, bool gles, std::string* unsupported);

// The per-stage constants for a draw: a stage's c0/c1 come from the
// SetPixelShaderConstant register its PSC0/PSC1 mapping nibble names (0xF:
// none) when bit r of `written` says that register was written since the
// shader was set, else from the definition's PSConstant0/1 (stage 0's for
// every stage unless the combiner count says the constants are unique), as
// SetPixelShader loads them (docs/design/20261017-i078-psh-definition-constants.md).
void StageConstants(const Definition& definition, const float pixel_constants[kPixelConstants][4], uint32_t written,
                    float c0[kStages][4], float c1[kStages][4], float final_c0[4], float final_c1[4]);

}  // namespace rexbox::hle::d3d8::psh

#endif  // REXBOX_HLE_D3D8_D3D8_PSH_H
