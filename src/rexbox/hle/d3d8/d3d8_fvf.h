// Flexible vertex format (FVF) codes to a vertex layout (pure, no GL). The
// Xbox keeps the PC D3D8 bit assignment (docs/kb/xbox-d3d8-api.md section 9);
// a vertex-shader handle with bit 0 set is a CreateVertexShader program, not
// an FVF.

#ifndef REXBOX_HLE_D3D8_D3D8_FVF_H
#define REXBOX_HLE_D3D8_D3D8_FVF_H

#include <cstdint>

namespace rexbox::hle::d3d8
{

namespace fvf
{
constexpr uint32_t kProgramHandle = 0x001;  // D3DFVF_RESERVED0: a vertex-shader program
constexpr uint32_t kPositionMask = 0x00E;
constexpr uint32_t kXyz = 0x002;
constexpr uint32_t kXyzRhw = 0x004;
constexpr uint32_t kXyzB1 = 0x006;
constexpr uint32_t kXyzB4 = 0x00C;
constexpr uint32_t kNormal = 0x010;
constexpr uint32_t kPointSize = 0x020;
constexpr uint32_t kDiffuse = 0x040;
constexpr uint32_t kSpecular = 0x080;
constexpr uint32_t kTexCountShift = 8;
constexpr uint32_t kTexCountMask = 0xF00;
constexpr uint32_t kTexCoordSizeShift = 16;  // 2 bits per set: 0 -> 2 floats, 1 -> 3, 2 -> 4, 3 -> 1
}  // namespace fvf

constexpr uint32_t kMaxTexCoords = 4;
constexpr int kAbsent = -1;

struct VertexLayout
{
    uint32_t stride = 0;
    bool pretransformed = false;     // XYZRHW: screen x, y, z and 1/w
    uint32_t position_components = 0;  // 3, or 4 for XYZRHW
    uint32_t blend_weights = 0;      // XYZB1..4: floats after the position
    int normal = kAbsent;            // byte offsets, kAbsent when missing
    int point_size = kAbsent;
    int diffuse = kAbsent;           // D3DCOLOR (B, G, R, A bytes)
    int specular = kAbsent;
    uint32_t tex_coords = 0;
    int tex_offset[kMaxTexCoords] = {kAbsent, kAbsent, kAbsent, kAbsent};
    uint32_t tex_components[kMaxTexCoords] = {};
};

// False when `code` is not a valid FVF (a program handle, no position, more
// than kMaxTexCoords sets).
bool DecodeFvf(uint32_t code, VertexLayout* layout);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_FVF_H
