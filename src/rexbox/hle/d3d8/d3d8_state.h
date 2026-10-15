// Pipeline state the title sets through the setters of the XDK 4134 library.
//
// Render and texture-stage state lives in the library's own globals
// (d3d8_globals.h): the setters write the slots the originals write, and the
// title's inlines write the rest directly. What the globals do not hold is
// kept here: texture bindings, transforms and shader handles.

#ifndef REXBOX_HLE_D3D8_D3D8_STATE_H
#define REXBOX_HLE_D3D8_D3D8_STATE_H

#include "d3d8_render_state.h"

#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"

#include <array>
#include <cstdint>
#include <vector>

namespace rexbox::hle::d3d8
{

constexpr uint32_t kMaxTextureStages = 4;
constexpr uint32_t kMaxTransforms = 32;  // D3DTRANSFORMSTATETYPE values the title uses (inferred bound)

// D3DTRANSFORMSTATETYPE (Xbox 4134; inferred, the title sets 1 every frame).
namespace ts
{
constexpr uint32_t kView = 0;
constexpr uint32_t kProjection = 1;
constexpr uint32_t kTexture0 = 2;  // .. kTexture3 = 5
constexpr uint32_t kWorld = 6;
}  // namespace ts

// D3DLIGHT8 (104 bytes, the PC layout; docs/kb/xbox-d3d8-api.md section 10)
// and whether LightEnable turned it on.
struct Light
{
    uint32_t type = 0;  // 1 point, 2 spot, 3 directional
    float diffuse[4] = {};
    float specular[4] = {};
    float ambient[4] = {};
    float position[3] = {};
    float direction[3] = {};
    float range = 0.0f;
    float falloff = 0.0f;
    float attenuation[3] = {};
    float theta = 0.0f;
    float phi = 0.0f;
    bool enabled = false;
};

// D3DMATERIAL8 (68 bytes).
struct Material
{
    float diffuse[4] = {};
    float ambient[4] = {};
    float specular[4] = {};
    float emissive[4] = {};
    float power = 0.0f;
};

constexpr uint32_t kMaxLights = 256;  // indices kept (inferred bound; the title uses few)

struct PipelineState
{
    std::array<runtime::GuestAddress, kMaxTextureStages> textures = {};  // bound texture headers
    std::array<std::array<float, 16>, kMaxTransforms> transforms = {};
    runtime::GuestAddress index_buffer = 0;
    uint32_t base_vertex_index = 0;
    uint32_t vertex_shader = 0;
    uint32_t pixel_shader = 0;
    uint32_t shader_constant_mode = 0;
    float screen_space_offset_x = 0.0f;
    float screen_space_offset_y = 0.0f;
    uint64_t texture_generation = 0;  // bumped by SetTexture and locks (d3d8_draw)
    std::vector<Light> lights;           // by index, grown by SetLight and LightEnable
    Material material;
};

PipelineState& Pipeline();

void RegisterStateExports(runtime::HleRegistry& registry);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_STATE_H
