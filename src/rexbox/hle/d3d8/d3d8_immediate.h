// Immediate-mode drawing (D3DDevice_Begin, SetVertexData*, End) as vertex
// bytes (pure, no GL; docs/design/20261015-i062-immediate-mode.md).
//
// Each SetVertexData call sets one register: a D3DVSDE value with an FVF, or
// the input register v0..v15 with a vertex-shader program (the D3DVSDE
// values are the same numbers). Registers keep their value across vertices
// and batches. Setting D3DVSDE_VERTEX sets the position (register 0) and
// completes a vertex with the current values of all registers.

#ifndef REXBOX_HLE_D3D8_D3D8_IMMEDIATE_H
#define REXBOX_HLE_D3D8_D3D8_IMMEDIATE_H

#include "d3d8_fvf.h"
#include "d3d8_vsh.h"

#include <cstdint>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace vsde
{
constexpr uint32_t kPosition = 0;
constexpr uint32_t kBlendWeight = 1;
constexpr uint32_t kNormal = 2;
constexpr uint32_t kDiffuse = 3;
constexpr uint32_t kSpecular = 4;
constexpr uint32_t kFog = 5;
constexpr uint32_t kPointSize = 6;
constexpr uint32_t kBackDiffuse = 7;
constexpr uint32_t kBackSpecular = 8;
constexpr uint32_t kTexCoord0 = 9;  // to kTexCoord0 + 3
constexpr uint32_t kVertex = 0xFFFFFFFF;
}  // namespace vsde

constexpr uint32_t kImmediateRegisters = 16;

struct ImmediateVertex
{
    float reg[kImmediateRegisters][4];
};

class ImmediateBatch
{
public:
    ImmediateBatch();

    // Starts a batch of `primitive` (a D3DPRIMITIVETYPE); vertices from an
    // unfinished batch are dropped.
    void Begin(uint32_t primitive);

    // Sets a register to (x, y, z, w); false (nothing changes) for a
    // register out of range. Outside a batch D3DVSDE_VERTEX only sets the
    // position.
    bool Set(uint32_t reg, float x, float y, float z, float w);

    // Ends the batch; its vertices stay readable until the next Begin.
    void End() { active_ = false; }

    bool active() const { return active_; }
    uint32_t primitive() const { return primitive_; }
    const std::vector<ImmediateVertex>& vertices() const { return vertices_; }

private:
    bool active_ = false;
    uint32_t primitive_ = 0;
    ImmediateVertex current_;
    std::vector<ImmediateVertex> vertices_;
};

// The vertices in an FVF's layout, `layout.stride` bytes each.
std::vector<uint8_t> PackFvfVertices(const VertexLayout& layout, const std::vector<ImmediateVertex>& vertices);

// The vertices in a program declaration's stream-0 layout, `stride` bytes
// each; registers the declaration does not place are left out.
std::vector<uint8_t> PackDeclarationVertices(const vsh::Declaration& declaration, uint32_t stride,
                                             const std::vector<ImmediateVertex>& vertices);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_IMMEDIATE_H
