#include "d3d8_state.h"

#include "d3d8_internal.h"

#include <cstring>

namespace rexbox::hle::d3d8
{

namespace
{

PipelineState g_pipeline;

// One setter per render state: void D3DDevice_SetRenderState_<Name>(DWORD Value).
template <RenderState kState>
void SetRenderState()
{
    HleCall call;
    g_pipeline.render_state[static_cast<size_t>(kState)] = call.Arg(0);
    call.Return(kD3dOk, 1);
}

// void D3DDevice_SetRenderStateNotInline(D3DRENDERSTATETYPE State, DWORD Value)
// The Xbox state numbering is kept raw until the drawing batch maps it.
void D3DDevice_SetRenderStateNotInline()
{
    HleCall call;
    const uint32_t state = call.Arg(0);
    if (state < g_pipeline.raw_render_state.size())
    {
        g_pipeline.raw_render_state[state] = call.Arg(1);
    }
    call.Return(kD3dOk, 2);
}

// void __fastcall D3DDevice_SetRenderState_Simple(DWORD Method, DWORD Value)
// Method is the NV2A register the library would write; the value is kept by
// register for the drawing batch (docs/design/20261012-i012 section 3).
void D3DDevice_SetRenderState_Simple()
{
    HleCall call;
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        std::fprintf(stderr, "[d3d8] SetRenderState_Simple method %08X value %08X (stored, not applied yet)\n",
                     call.Ecx(), call.Edx());
    }
    call.Return(kD3dOk, 0);
}

TextureStageState* Stage(uint32_t stage)
{
    return stage < kMaxTextureStages ? &g_pipeline.stages[stage] : nullptr;
}

// void D3DDevice_SetTextureStageStateNotInline(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
void D3DDevice_SetTextureStageStateNotInline()
{
    HleCall call;
    TextureStageState* stage = Stage(call.Arg(0));
    const uint32_t type = call.Arg(1);
    if (stage != nullptr && type < stage->raw.size())
    {
        stage->raw[type] = call.Arg(2);
    }
    call.Return(kD3dOk, 3);
}

// void D3DDevice_SetTextureState_TexCoordIndex(DWORD Stage, DWORD Value)
void D3DDevice_SetTextureState_TexCoordIndex()
{
    HleCall call;
    if (TextureStageState* stage = Stage(call.Arg(0)))
    {
        stage->tex_coord_index = call.Arg(1);
    }
    call.Return(kD3dOk, 2);
}

// void D3DDevice_SetTextureState_BorderColor(DWORD Stage, D3DCOLOR Color)
void D3DDevice_SetTextureState_BorderColor()
{
    HleCall call;
    if (TextureStageState* stage = Stage(call.Arg(0)))
    {
        stage->border_color = call.Arg(1);
    }
    call.Return(kD3dOk, 2);
}

// void D3DDevice_SetTextureState_ColorKeyColor(DWORD Stage, D3DCOLOR Color)
void D3DDevice_SetTextureState_ColorKeyColor()
{
    HleCall call;
    if (TextureStageState* stage = Stage(call.Arg(0)))
    {
        stage->color_key_color = call.Arg(1);
    }
    call.Return(kD3dOk, 2);
}

// void D3DDevice_SetTextureState_BumpEnv(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
void D3DDevice_SetTextureState_BumpEnv()
{
    HleCall call;
    TextureStageState* stage = Stage(call.Arg(0));
    const uint32_t type = call.Arg(1);
    if (stage != nullptr && type < stage->bump_env.size())
    {
        stage->bump_env[type] = call.Arg(2);
    }
    call.Return(kD3dOk, 3);
}

// void D3DDevice_SetTexture(DWORD Stage, D3DBaseTexture* pTexture)
void D3DDevice_SetTexture()
{
    HleCall call;
    if (TextureStageState* stage = Stage(call.Arg(0)))
    {
        stage->texture = call.Arg(1);
    }
    call.Return(kD3dOk, 2);
}

// void D3DDevice_SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix)
void D3DDevice_SetTransform()
{
    HleCall call;
    const uint32_t state = call.Arg(0);
    const GuestAddress matrix = call.Arg(1);
    if (state < kMaxTransforms && matrix != 0)
    {
        std::memcpy(g_pipeline.transforms[state].data(), Memory().Translate(matrix), 64);
    }
    call.Return(kD3dOk, 2);
}

// void D3DDevice_SetStreamSource(UINT StreamNumber, D3DVertexBuffer* pStreamData, UINT Stride)
void D3DDevice_SetStreamSource()
{
    HleCall call;
    const uint32_t stream = call.Arg(0);
    if (stream < kMaxStreams)
    {
        g_pipeline.streams[stream].vertex_buffer = call.Arg(1);
        g_pipeline.streams[stream].stride = call.Arg(2);
    }
    call.Return(kD3dOk, 3);
}

// void D3DDevice_SetIndices(D3DIndexBuffer* pIndexData, UINT BaseVertexIndex)
void D3DDevice_SetIndices()
{
    HleCall call;
    g_pipeline.index_buffer = call.Arg(0);
    g_pipeline.base_vertex_index = call.Arg(1);
    call.Return(kD3dOk, 2);
}

// void D3DDevice_SetVertexShader(DWORD Handle)
void D3DDevice_SetVertexShader()
{
    HleCall call;
    g_pipeline.vertex_shader = call.Arg(0);
    call.Return(kD3dOk, 1);
}

// void D3DDevice_SetPixelShader(DWORD Handle)
void D3DDevice_SetPixelShader()
{
    HleCall call;
    g_pipeline.pixel_shader = call.Arg(0);
    call.Return(kD3dOk, 1);
}

// void D3DDevice_SetShaderConstantMode(D3DSHADERCONSTANTMODE Mode)
void D3DDevice_SetShaderConstantMode()
{
    HleCall call;
    g_pipeline.shader_constant_mode = call.Arg(0);
    call.Return(kD3dOk, 1);
}

// void D3DDevice_SetScreenSpaceOffset(float x, float y)
void D3DDevice_SetScreenSpaceOffset()
{
    HleCall call;
    float x = 0.0f;
    float y = 0.0f;
    uint32_t bits = call.Arg(0);
    std::memcpy(&x, &bits, 4);
    bits = call.Arg(1);
    std::memcpy(&y, &bits, 4);
    g_pipeline.screen_space_offset_x = static_cast<int32_t>(x);
    g_pipeline.screen_space_offset_y = static_cast<int32_t>(y);
    call.Return(kD3dOk, 2);
}

// Setters whose arguments describe fixed-function lighting or material; the
// values are accepted (the title's rendering uses vertex shaders, to be
// confirmed in the drawing batch).
void AcceptOne()
{
    HleCall call;
    call.Return(kD3dOk, 1);
}

void AcceptTwo()
{
    HleCall call;
    call.Return(kD3dOk, 2);
}

void AcceptThree()
{
    HleCall call;
    call.Return(kD3dOk, 3);
}

}  // namespace

PipelineState& Pipeline()
{
    return g_pipeline;
}

void RegisterStateExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_BackFillMode", &SetRenderState<RenderState::kBackFillMode>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_CullMode", &SetRenderState<RenderState::kCullMode>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_DoNotCullUncompressed",
                       &SetRenderState<RenderState::kDoNotCullUncompressed>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_Dxt1NoiseEnable", &SetRenderState<RenderState::kDxt1NoiseEnable>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_EdgeAntiAlias", &SetRenderState<RenderState::kEdgeAntiAlias>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_FillMode", &SetRenderState<RenderState::kFillMode>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_FogColor", &SetRenderState<RenderState::kFogColor>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_FrontFace", &SetRenderState<RenderState::kFrontFace>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_LineWidth", &SetRenderState<RenderState::kLineWidth>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_LogicOp", &SetRenderState<RenderState::kLogicOp>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_MultiSampleAntiAlias",
                       &SetRenderState<RenderState::kMultiSampleAntiAlias>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_MultiSampleMask", &SetRenderState<RenderState::kMultiSampleMask>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_MultiSampleMode", &SetRenderState<RenderState::kMultiSampleMode>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_MultiSampleRenderTargetMode",
                       &SetRenderState<RenderState::kMultiSampleRenderTargetMode>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_NormalizeNormals",
                       &SetRenderState<RenderState::kNormalizeNormals>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_OcclusionCullEnable",
                       &SetRenderState<RenderState::kOcclusionCullEnable>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_PSTextureModes", &SetRenderState<RenderState::kPSTextureModes>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_RopZCmpAlwaysRead",
                       &SetRenderState<RenderState::kRopZCmpAlwaysRead>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_RopZRead", &SetRenderState<RenderState::kRopZRead>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_ShadowFunc", &SetRenderState<RenderState::kShadowFunc>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_StencilCullEnable",
                       &SetRenderState<RenderState::kStencilCullEnable>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_StencilEnable", &SetRenderState<RenderState::kStencilEnable>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_StencilFail", &SetRenderState<RenderState::kStencilFail>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_TextureFactor", &SetRenderState<RenderState::kTextureFactor>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_TwoSidedLighting",
                       &SetRenderState<RenderState::kTwoSidedLighting>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_VertexBlend", &SetRenderState<RenderState::kVertexBlend>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_YuvEnable", &SetRenderState<RenderState::kYuvEnable>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_ZBias", &SetRenderState<RenderState::kZBias>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_ZEnable", &SetRenderState<RenderState::kZEnable>);
    registry.AddExport("d3d8", "D3DDevice_SetRenderStateNotInline", &D3DDevice_SetRenderStateNotInline);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_Simple", &D3DDevice_SetRenderState_Simple);
    registry.AddExport("d3d8", "D3DDevice_SetTextureStageStateNotInline", &D3DDevice_SetTextureStageStateNotInline);
    registry.AddExport("d3d8", "D3DDevice_SetTextureState_TexCoordIndex", &D3DDevice_SetTextureState_TexCoordIndex);
    registry.AddExport("d3d8", "D3DDevice_SetTextureState_BorderColor", &D3DDevice_SetTextureState_BorderColor);
    registry.AddExport("d3d8", "D3DDevice_SetTextureState_ColorKeyColor", &D3DDevice_SetTextureState_ColorKeyColor);
    registry.AddExport("d3d8", "D3DDevice_SetTextureState_BumpEnv", &D3DDevice_SetTextureState_BumpEnv);
    registry.AddExport("d3d8", "D3DDevice_SetTexture", &D3DDevice_SetTexture);
    registry.AddExport("d3d8", "D3DDevice_SetTransform", &D3DDevice_SetTransform);
    registry.AddExport("d3d8", "D3DDevice_SetStreamSource", &D3DDevice_SetStreamSource);
    registry.AddExport("d3d8", "D3DDevice_SetIndices", &D3DDevice_SetIndices);
    registry.AddExport("d3d8", "D3DDevice_SetVertexShader", &D3DDevice_SetVertexShader);
    registry.AddExport("d3d8", "D3DDevice_SetPixelShader", &D3DDevice_SetPixelShader);
    registry.AddExport("d3d8", "D3DDevice_SetShaderConstantMode", &D3DDevice_SetShaderConstantMode);
    registry.AddExport("d3d8", "D3DDevice_SetScreenSpaceOffset", &D3DDevice_SetScreenSpaceOffset);
    registry.AddExport("d3d8", "D3DDevice_SetMaterial", &AcceptOne);
    registry.AddExport("d3d8", "D3DDevice_SetLight", &AcceptTwo);
    registry.AddExport("d3d8", "D3DDevice_SetScissors", &AcceptThree);
}

}  // namespace rexbox::hle::d3d8
