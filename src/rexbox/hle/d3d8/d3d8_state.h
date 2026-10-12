// Pipeline state the title sets through the per-state setters of the XDK
// 4134 library (D3DDevice_SetRenderState_*, SetTextureState_*, SetTransform,
// shader and stream bindings). This batch stores the state; the drawing
// batch consumes it.

#ifndef REXBOX_HLE_D3D8_D3D8_STATE_H
#define REXBOX_HLE_D3D8_D3D8_STATE_H

#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"

#include <array>
#include <cstdint>

namespace rexbox::hle::d3d8
{

// Render states with a dedicated setter in the library, in the order of the
// setters below (our own numbering; the Xbox D3DRENDERSTATETYPE values differ).
enum class RenderState : uint32_t
{
    kBackFillMode,
    kCullMode,
    kDoNotCullUncompressed,
    kDxt1NoiseEnable,
    kEdgeAntiAlias,
    kFillMode,
    kFogColor,
    kFrontFace,
    kLineWidth,
    kLogicOp,
    kMultiSampleAntiAlias,
    kMultiSampleMask,
    kMultiSampleMode,
    kMultiSampleRenderTargetMode,
    kNormalizeNormals,
    kOcclusionCullEnable,
    kPSTextureModes,
    kRopZCmpAlwaysRead,
    kRopZRead,
    kShadowFunc,
    kStencilCullEnable,
    kStencilEnable,
    kStencilFail,
    kTextureFactor,
    kTwoSidedLighting,
    kVertexBlend,
    kYuvEnable,
    kZBias,
    kZEnable,
    kCount,
};

constexpr uint32_t kMaxTextureStages = 4;
constexpr uint32_t kMaxStreams = 16;
constexpr uint32_t kMaxTransforms = 32;  // D3DTS_* indices used by the title (world, view, projection, textures)

struct TextureStageState
{
    runtime::GuestAddress texture = 0;  // header of the bound texture, 0 for none
    uint32_t tex_coord_index = 0;
    uint32_t border_color = 0;
    uint32_t color_key_color = 0;
    std::array<uint32_t, 4> bump_env = {};
    std::array<uint32_t, 64> raw = {};  // SetTextureStageStateNotInline(Stage, Type, Value) by Type
};

struct StreamSource
{
    runtime::GuestAddress vertex_buffer = 0;
    uint32_t stride = 0;
};

struct PipelineState
{
    std::array<uint32_t, static_cast<size_t>(RenderState::kCount)> render_state = {};
    std::array<uint32_t, 256> raw_render_state = {};  // SetRenderStateNotInline(State, Value) by State
    std::array<TextureStageState, kMaxTextureStages> stages;
    std::array<StreamSource, kMaxStreams> streams;
    std::array<std::array<float, 16>, kMaxTransforms> transforms = {};
    runtime::GuestAddress index_buffer = 0;
    uint32_t base_vertex_index = 0;
    uint32_t vertex_shader = 0;
    uint32_t pixel_shader = 0;
    uint32_t shader_constant_mode = 0;
    int32_t screen_space_offset_x = 0;
    int32_t screen_space_offset_y = 0;
};

PipelineState& Pipeline();

void RegisterStateExports(runtime::HleRegistry& registry);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_STATE_H
