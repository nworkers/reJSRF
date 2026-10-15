// Fixed-function pipeline as GLSL (pure, no GL; verified by rexbox_d3d8_probe).
// A key captures everything that changes the program text: the vertex
// layout, the active texture stages and the per-fragment tests that GL 3.3
// core no longer has (alpha test). Values that only change numbers (matrices,
// texture factor, alpha reference) are uniforms.
//
// Texture-stage semantics follow D3D8's (D3DTOP_*, D3DTA_*; the Xbox keeps
// the PC numbering, docs/kb/xbox-d3d8-api.md section 10). A stage whose
// texture is unbound samples opaque white (inferred), so the Xbox default of
// MODULATE on every stage leaves untextured stages without effect.

#ifndef REXBOX_HLE_D3D8_D3D8_FF_SHADER_H
#define REXBOX_HLE_D3D8_D3D8_FF_SHADER_H

#include <array>
#include <cstdint>
#include <string>

namespace rexbox::hle::d3d8
{

namespace top
{
constexpr uint32_t kDisable = 1;
constexpr uint32_t kSelectArg1 = 2;
constexpr uint32_t kSelectArg2 = 3;
constexpr uint32_t kModulate = 4;
constexpr uint32_t kModulate2x = 5;
constexpr uint32_t kModulate4x = 6;
constexpr uint32_t kAdd = 7;
constexpr uint32_t kAddSigned = 8;
constexpr uint32_t kAddSigned2x = 9;
constexpr uint32_t kSubtract = 10;
constexpr uint32_t kAddSmooth = 11;
constexpr uint32_t kBlendDiffuseAlpha = 12;
constexpr uint32_t kBlendCurrentAlpha = 13;
constexpr uint32_t kBlendTextureAlpha = 14;
constexpr uint32_t kBlendFactorAlpha = 15;
constexpr uint32_t kBlendTextureAlphaPm = 16;
constexpr uint32_t kPremodulate = 17;
constexpr uint32_t kModulateAlphaAddColor = 18;
constexpr uint32_t kModulateColorAddAlpha = 19;
constexpr uint32_t kModulateInvAlphaAddColor = 20;
constexpr uint32_t kModulateInvColorAddAlpha = 21;
constexpr uint32_t kDotProduct3 = 22;
constexpr uint32_t kMultiplyAdd = 23;
constexpr uint32_t kLerp = 24;
constexpr uint32_t kBumpEnvMap = 25;
constexpr uint32_t kBumpEnvMapLuminance = 26;
}  // namespace top

namespace ta
{
constexpr uint32_t kSelectMask = 0x0F;
constexpr uint32_t kDiffuse = 0;
constexpr uint32_t kCurrent = 1;
constexpr uint32_t kTexture = 2;
constexpr uint32_t kTFactor = 3;
constexpr uint32_t kSpecular = 4;
constexpr uint32_t kTemp = 5;
constexpr uint32_t kComplement = 0x10;
constexpr uint32_t kAlphaReplicate = 0x20;
}  // namespace ta

// Texture coordinate generation: the high 16 bits of D3DTSS_TEXCOORDINDEX
// (D3DTSS_TCI_*, PC values).
namespace texgen
{
constexpr uint8_t kNone = 0;  // the vertex set named by the low 16 bits
constexpr uint8_t kCameraSpaceNormal = 1;
constexpr uint8_t kCameraSpacePosition = 2;
constexpr uint8_t kCameraSpaceReflection = 3;
}  // namespace texgen

// D3DTSS_TEXTURETRANSFORMFLAGS (D3DTTFF_*, PC values).
namespace ttff
{
constexpr uint32_t kCountMask = 0xFF;  // COUNT1..4: output components; 0 disables
constexpr uint32_t kProjected = 0x100;   // divide by the last output component
}  // namespace ttff

// D3DLIGHTTYPE.
namespace light
{
constexpr uint8_t kPoint = 1;
constexpr uint8_t kSpot = 2;
constexpr uint8_t kDirectional = 3;
}  // namespace light

// Where a lit color comes from (D3DMATERIALCOLORSOURCE, resolved against the
// vertex format: a missing vertex color falls back to the material).
namespace msrc
{
constexpr uint8_t kMaterial = 0;
constexpr uint8_t kDiffuse = 1;   // the vertex diffuse color
constexpr uint8_t kSpecular = 2;  // the vertex specular color
}  // namespace msrc

// Material colors in FixedFunctionKey::material_source order.
namespace mcolor
{
constexpr uint32_t kDiffuse = 0;
constexpr uint32_t kAmbient = 1;
constexpr uint32_t kSpecular = 2;
constexpr uint32_t kEmissive = 3;
}  // namespace mcolor

constexpr uint32_t kMaxActiveLights = 8;

enum class SamplerKind : uint8_t
{
    kNone,  // unbound: reads opaque white
    k2D,
    kCube,
    k3D,
};

constexpr uint32_t kFixedFunctionStages = 4;

struct FixedFunctionKey
{
    // Vertex input.
    bool pretransformed = false;
    bool has_diffuse = false;
    bool has_specular = false;
    uint8_t tex_coords = 0;
    uint8_t tex_components[4] = {};

    struct Stage
    {
        uint32_t color_op = top::kDisable;
        uint32_t color_arg[3] = {ta::kCurrent, ta::kTexture, ta::kCurrent};  // arg0, arg1, arg2
        uint32_t alpha_op = top::kDisable;
        uint32_t alpha_arg[3] = {ta::kCurrent, ta::kTexture, ta::kCurrent};
        uint32_t result_arg = ta::kCurrent;
        uint8_t tex_coord_index = 0;  // which vertex texture-coordinate set
        // Fixed-function vertex processing only (stage_coords): the generated
        // input (texgen::*) and the transform flags (ttff count and
        // projected bit, 0 when the transform is off).
        uint8_t tex_gen = texgen::kNone;
        uint16_t tex_transform = 0;
        SamplerKind sampler = SamplerKind::kNone;
        // Linear (unswizzled) textures are sampled with texel coordinates on
        // the NV2A (inferred); the shader scales them by u_tex_scale<stage>.
        bool unnormalized = false;

        bool operator==(const Stage&) const = default;
    };
    Stage stages[kFixedFunctionStages];

    // The vertex shader writes one coordinate per active stage (v_tex<s>),
    // generated and transformed as the stage says; otherwise (a vertex
    // program supplies v_tex0..3) a stage reads v_tex<tex_coord_index>.
    bool stage_coords = false;
    bool has_normal = false;  // a vertex normal the generation or lighting reads
    bool normalize_normals = false;

    // Fixed-function lighting (stage_coords draws with D3DRS_LIGHTING): the
    // enabled lights' types in index order, where each lit color comes
    // from, and D3DRS_LOCALVIEWER. specular_enable also turns on the
    // specular term.
    bool lighting = false;
    bool local_viewer = false;
    uint8_t light_count = 0;
    uint8_t light_types[kMaxActiveLights] = {};
    uint8_t material_source[4] = {};  // mcolor order: msrc values

    bool specular_enable = false;
    uint32_t alpha_func = 0;  // 0: alpha test off; else the comparison (0x200 NEVER .. 0x207 ALWAYS)

    // A readable string that is equal for equal keys (logs and dumps).
    std::string Serialize() const;
    // The program caches compare keys field by field and bucket by Hash.
    bool operator==(const FixedFunctionKey&) const = default;
    uint64_t Hash() const;
};

// One step of the 64-bit hash the program keys use.
inline uint64_t HashMix(uint64_t hash, uint64_t value)
{
    return (hash ^ value) * 1099511628211ull;
}

// Attribute locations the programs use.
namespace attr
{
constexpr uint32_t kPosition = 0;
constexpr uint32_t kDiffuse = 1;
constexpr uint32_t kSpecular = 2;
constexpr uint32_t kTexCoord0 = 3;  // .. kTexCoord0 + 3
constexpr uint32_t kNormal = 7;
}  // namespace attr

// GLSL sources for `key`. `gles` selects "#version 300 es" over "330 core".
// Uniforms: u_transform (mat4, World*View*Projection, row-vector convention),
// u_target (vec4: render-target width, height, screen-space offset x, y),
// u_tex0..3, u_tex_scale0..3 (vec2, 1/size for unnormalized stages),
// u_tfactor (vec4), u_alpha_ref (float, 0..255); for coordinate generation
// u_world_view (mat4, World*View) and u_normal_matrix (mat4, NormalMatrix),
// for transforms u_tex_matrix0..3 (mat4, D3DTS_TEXTURE<s>); for lighting
// u_material[4] (mcolor order), u_material_power, u_ambient (D3DRS_AMBIENT),
// and per light (camera space) u_light_diffuse/specular/ambient[8],
// u_light_position[8] (xyz, w = range), u_light_direction[8] (xyz, unit,
// pointing away from the light), u_light_attenuation[8] (att0..2, falloff)
// and u_light_cone[8] (cos(theta/2), cos(phi/2)). Matrices are D3D's
// row-major arrays as given (row vectors: v * M).
std::string GenerateFixedFunctionVertexShader(const FixedFunctionKey& key, bool gles);
std::string GenerateFixedFunctionFragmentShader(const FixedFunctionKey& key, bool gles);

// The matrix that takes object-space normals to camera space for the
// row-major `world_view`: the inverse transpose of its upper 3x3, padded to
// 4x4 (the 3x3 itself when it is singular).
std::array<float, 16> NormalMatrix(const std::array<float, 16>& world_view);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_FF_SHADER_H
