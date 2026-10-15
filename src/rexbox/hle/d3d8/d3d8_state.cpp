#include "d3d8_state.h"

#include "d3d8_globals.h"
#include "d3d8_internal.h"
#include "d3d8_resource.h"
#include "d3d8_trace.h"

#include <cstring>

namespace rexbox::hle::d3d8
{

namespace
{

PipelineState g_pipeline;

uint32_t FloatBits(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    return bits;
}

float FloatArg(uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, 4);
    return value;
}

// One setter per complex render state: void D3DDevice_SetRenderState_<Name>(DWORD Value).
// The original stores Value in its D3D__RenderState slot (and pushes the
// matching NV2A methods, which the GL backend derives at draw time instead).
template <uint32_t kSlot>
void SetComplexRenderState()
{
    HleCall call;
    SetRenderStateValue(kSlot, call.Arg(0));
    call.Return(kD3dOk, 1);
}

// D3DRS_ZBIAS also sets the polygon-offset states (the original goes through
// SetRenderStateNotInline): slope -Value/4, offset -Value, the three enables
// to Value != 0 (measured with rejsrf_d3d8_check).
void ApplyZBias(uint32_t value)
{
    const float bias = static_cast<float>(value);
    SetRenderStateValue(rs::kZBias, value);
    SetRenderStateValue(rs::kPolygonOffsetZSlopeScale, FloatBits(-bias / 4.0f));
    SetRenderStateValue(rs::kPolygonOffsetZOffset, FloatBits(-bias));
    const uint32_t enable = value != 0 ? 1u : 0u;
    SetRenderStateValue(rs::kPointOffsetEnable, enable);
    SetRenderStateValue(rs::kWireframeOffsetEnable, enable);
    SetRenderStateValue(rs::kSolidOffsetEnable, enable);
}

// void D3DDevice_SetRenderState_ZBias(DWORD Value)
void D3DDevice_SetRenderState_ZBias()
{
    HleCall call;
    ApplyZBias(call.Arg(0));
    call.Return(kD3dOk, 1);
}

// void __fastcall D3DDevice_SetRenderState_Simple(DWORD Method, DWORD Value)
// The title's inline already stored the D3D__RenderState slot, which drawing
// reads; Method and Value only feed the original's push buffer.
void D3DDevice_SetRenderState_Simple()
{
    HleCall call;
    call.Return(kD3dOk, 0);
}

// void D3DDevice_SetRenderStateNotInline(D3DRENDERSTATETYPE State, DWORD Value)
// The original stores the slot and, for the states below the deferred block,
// pushes the NV2A method from its own table; those states reach the GL
// backend through the slot (d3d8_draw reads them).
void D3DDevice_SetRenderStateNotInline()
{
    HleCall call;
    // Complex states go through their setters in the original; ZBias is the
    // only one whose setter writes more than its own slot.
    if (call.Arg(0) == rs::kZBias)
    {
        ApplyZBias(call.Arg(1));
    }
    else
    {
        SetRenderStateValue(call.Arg(0), call.Arg(1));
    }
    call.Return(kD3dOk, 2);
}

// void D3DDevice_SetTextureStageStateNotInline(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
// and the dedicated setters below: every path of the original ends in
// D3D__TextureState[Stage][Type] = Value.
void D3DDevice_SetTextureStageStateNotInline()
{
    HleCall call;
    SetTextureStateValue(call.Arg(0), call.Arg(1), call.Arg(2));
    call.Return(kD3dOk, 3);
}

template <uint32_t kSlot>
void SetTextureStateSlot()
{
    HleCall call;
    SetTextureStateValue(call.Arg(0), kSlot, call.Arg(1));
    call.Return(kD3dOk, 2);
}

// void D3DDevice_SetTextureState_BumpEnv(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
void D3DDevice_SetTextureState_BumpEnv()
{
    HleCall call;
    SetTextureStateValue(call.Arg(0), call.Arg(1), call.Arg(2));
    call.Return(kD3dOk, 3);
}

// void D3DDevice_SetTexture(DWORD Stage, D3DBaseTexture* pTexture)
// The device holds a binding on the texture (internal reference count).
void D3DDevice_SetTexture()
{
    HleCall call;
    const uint32_t stage = call.Arg(0);
    const GuestAddress texture = call.Arg(1);
    if (stage < kMaxTextureStages)
    {
        const GuestAddress previous = g_pipeline.textures[stage];
        if (texture != 0)
        {
            AddResourceBinding(texture);
            if (D3d8TraceEnabled())
            {
                const uint32_t format_field = Memory().Read32(texture + header::kFormat);
                const uint32_t size_field = Memory().Read32(texture + header::kSize);
                const uint32_t common = Memory().Read32(texture + header::kCommon);
                TraceOnce(Format("texture %08X %08X", format_field & 0xFFFFFF0C, size_field != 0 ? 1u : 0u),
                          Format("texture format %02X dim %u levels %u cube %u %s, type %X (first: header %08X, "
                                 "Format %08X Size %08X)",
                                 HeaderFormat(texture), (format_field >> header::kFormatDimensionShift) & 0xF,
                                 (format_field >> header::kFormatMipLevelsShift) & 0xF,
                                 (format_field & header::kFormatCubeMap) != 0 ? 1u : 0u,
                                 size_field != 0 ? "linear" : "swizzled", (common & header::kTypeMask) >> 16,
                                 texture, format_field, size_field));
            }
        }
        g_pipeline.textures[stage] = texture;
        if (previous != 0)
        {
            ReleaseResourceBinding(previous, DevicePushFence());
        }
        ++g_pipeline.texture_generation;
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
    const GuestAddress buffer = call.Arg(1);
    if (stream < globals::kStreamCount)
    {
        if (buffer != 0)
        {
            AddResourceBinding(buffer);
        }
        const GuestAddress previous = StreamVertexBuffer(stream);
        if (previous != 0)
        {
            ReleaseResourceBinding(previous, DevicePushFence());
        }
        SetStream(stream, buffer, call.Arg(2));
    }
    call.Return(kD3dOk, 3);
}

// void D3DDevice_SetIndices(D3DIndexBuffer* pIndexData, UINT BaseVertexIndex)
// The title's DrawIndexedPrimitive inline reads D3D__IndexData itself.
void D3DDevice_SetIndices()
{
    HleCall call;
    const GuestAddress buffer = call.Arg(0);
    if (buffer != 0)
    {
        AddResourceBinding(buffer);
    }
    SetIndexData(buffer != 0 ? Memory().Read32(buffer + header::kData) : 0);
    const GuestAddress previous = g_pipeline.index_buffer;
    if (previous != 0)
    {
        ReleaseResourceBinding(previous);
    }
    g_pipeline.index_buffer = buffer;
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
    g_pipeline.screen_space_offset_x = FloatArg(call.Arg(0));
    g_pipeline.screen_space_offset_y = FloatArg(call.Arg(1));
    call.Return(kD3dOk, 2);
}

Light* LightAt(uint32_t index)
{
    if (index >= kMaxLights)
    {
        ReportOncePer("light index", 0, [index] { return Format("light index %u is beyond the lights kept", index); });
        return nullptr;
    }
    if (index >= g_pipeline.lights.size())
    {
        g_pipeline.lights.resize(index + 1);
    }
    return &g_pipeline.lights[index];
}

void ReadFloats(GuestAddress address, float* out, uint32_t count)
{
    std::memcpy(out, Memory().Translate(address), 4u * count);
}

// HRESULT D3DDevice_SetLight(DWORD Index, const D3DLIGHT8* pLight)
void D3DDevice_SetLight()
{
    HleCall call;
    const GuestAddress source = call.Arg(1);
    Light* light = LightAt(call.Arg(0));
    if (light != nullptr && source != 0 && Memory().IsValid(source, 104))
    {
        light->type = Memory().Read32(source);
        ReadFloats(source + 4, light->diffuse, 4);
        ReadFloats(source + 20, light->specular, 4);
        ReadFloats(source + 36, light->ambient, 4);
        ReadFloats(source + 52, light->position, 3);
        ReadFloats(source + 64, light->direction, 3);
        ReadFloats(source + 76, &light->range, 1);
        ReadFloats(source + 80, &light->falloff, 1);
        ReadFloats(source + 84, light->attenuation, 3);
        ReadFloats(source + 96, &light->theta, 1);
        ReadFloats(source + 100, &light->phi, 1);
    }
    call.Return(kD3dOk, 2);
}

// HRESULT D3DDevice_LightEnable(DWORD Index, BOOL bEnable)
void D3DDevice_LightEnable()
{
    HleCall call;
    Light* light = LightAt(call.Arg(0));
    if (light != nullptr)
    {
        light->enabled = call.Arg(1) != 0;
    }
    call.Return(kD3dOk, 2);
}

// HRESULT D3DDevice_SetMaterial(const D3DMATERIAL8* pMaterial)
void D3DDevice_SetMaterial()
{
    HleCall call;
    const GuestAddress source = call.Arg(0);
    if (source != 0 && Memory().IsValid(source, 68))
    {
        Material& material = g_pipeline.material;
        ReadFloats(source, material.diffuse, 4);
        ReadFloats(source + 16, material.ambient, 4);
        ReadFloats(source + 32, material.specular, 4);
        ReadFloats(source + 48, material.emissive, 4);
        ReadFloats(source + 64, &material.power, 1);
    }
    call.Return(kD3dOk, 1);
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
    struct ComplexSetter
    {
        const char* name;
        void (*function)();
    };
    static const ComplexSetter kComplexSetters[] = {
        {"D3DDevice_SetRenderState_PSTextureModes", &SetComplexRenderState<rs::kPSTextureModes>},
        {"D3DDevice_SetRenderState_VertexBlend", &SetComplexRenderState<rs::kVertexBlend>},
        {"D3DDevice_SetRenderState_FogColor", &SetComplexRenderState<rs::kFogColor>},
        {"D3DDevice_SetRenderState_FillMode", &SetComplexRenderState<rs::kFillMode>},
        {"D3DDevice_SetRenderState_BackFillMode", &SetComplexRenderState<rs::kBackFillMode>},
        {"D3DDevice_SetRenderState_TwoSidedLighting", &SetComplexRenderState<rs::kTwoSidedLighting>},
        {"D3DDevice_SetRenderState_NormalizeNormals", &SetComplexRenderState<rs::kNormalizeNormals>},
        {"D3DDevice_SetRenderState_ZEnable", &SetComplexRenderState<rs::kZEnable>},
        {"D3DDevice_SetRenderState_StencilEnable", &SetComplexRenderState<rs::kStencilEnable>},
        {"D3DDevice_SetRenderState_StencilFail", &SetComplexRenderState<rs::kStencilFail>},
        {"D3DDevice_SetRenderState_FrontFace", &SetComplexRenderState<rs::kFrontFace>},
        {"D3DDevice_SetRenderState_CullMode", &SetComplexRenderState<rs::kCullMode>},
        {"D3DDevice_SetRenderState_TextureFactor", &SetComplexRenderState<rs::kTextureFactor>},
        {"D3DDevice_SetRenderState_LogicOp", &SetComplexRenderState<rs::kLogicOp>},
        {"D3DDevice_SetRenderState_EdgeAntiAlias", &SetComplexRenderState<rs::kEdgeAntiAlias>},
        {"D3DDevice_SetRenderState_MultiSampleAntiAlias", &SetComplexRenderState<rs::kMultiSampleAntiAlias>},
        {"D3DDevice_SetRenderState_MultiSampleMask", &SetComplexRenderState<rs::kMultiSampleMask>},
        {"D3DDevice_SetRenderState_MultiSampleMode", &SetComplexRenderState<rs::kMultiSampleMode>},
        {"D3DDevice_SetRenderState_MultiSampleRenderTargetMode",
         &SetComplexRenderState<rs::kMultiSampleRenderTargetMode>},
        {"D3DDevice_SetRenderState_ShadowFunc", &SetComplexRenderState<rs::kShadowFunc>},
        {"D3DDevice_SetRenderState_LineWidth", &SetComplexRenderState<rs::kLineWidth>},
        {"D3DDevice_SetRenderState_Dxt1NoiseEnable", &SetComplexRenderState<rs::kDxt1NoiseEnable>},
        {"D3DDevice_SetRenderState_YuvEnable", &SetComplexRenderState<rs::kYuvEnable>},
        {"D3DDevice_SetRenderState_OcclusionCullEnable", &SetComplexRenderState<rs::kOcclusionCullEnable>},
        {"D3DDevice_SetRenderState_StencilCullEnable", &SetComplexRenderState<rs::kStencilCullEnable>},
        {"D3DDevice_SetRenderState_RopZCmpAlwaysRead", &SetComplexRenderState<rs::kRopZCmpAlwaysRead>},
        {"D3DDevice_SetRenderState_RopZRead", &SetComplexRenderState<rs::kRopZRead>},
        {"D3DDevice_SetRenderState_DoNotCullUncompressed", &SetComplexRenderState<rs::kDoNotCullUncompressed>},
    };
    for (const ComplexSetter& setter : kComplexSetters)
    {
        registry.AddExport("d3d8", setter.name, setter.function);
    }
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_ZBias", &D3DDevice_SetRenderState_ZBias);
    registry.AddExport("d3d8", "D3DDevice_SetRenderStateNotInline", &D3DDevice_SetRenderStateNotInline);
    registry.AddExport("d3d8", "D3DDevice_SetRenderState_Simple", &D3DDevice_SetRenderState_Simple);
    registry.AddExport("d3d8", "D3DDevice_SetTextureStageStateNotInline", &D3DDevice_SetTextureStageStateNotInline);
    registry.AddExport("d3d8", "D3DDevice_SetTextureState_TexCoordIndex", &SetTextureStateSlot<tss::kTexCoordIndex>);
    registry.AddExport("d3d8", "D3DDevice_SetTextureState_BorderColor", &SetTextureStateSlot<tss::kBorderColor>);
    registry.AddExport("d3d8", "D3DDevice_SetTextureState_ColorKeyColor", &SetTextureStateSlot<tss::kColorKeyColor>);
    registry.AddExport("d3d8", "D3DDevice_SetTextureState_BumpEnv", &D3DDevice_SetTextureState_BumpEnv);
    registry.AddExport("d3d8", "D3DDevice_SetTexture", &D3DDevice_SetTexture);
    registry.AddExport("d3d8", "D3DDevice_SetTransform", &D3DDevice_SetTransform);
    registry.AddExport("d3d8", "D3DDevice_SetStreamSource", &D3DDevice_SetStreamSource);
    registry.AddExport("d3d8", "D3DDevice_SetIndices", &D3DDevice_SetIndices);
    registry.AddExport("d3d8", "D3DDevice_SetVertexShader", &D3DDevice_SetVertexShader);
    registry.AddExport("d3d8", "D3DDevice_SetPixelShader", &D3DDevice_SetPixelShader);
    registry.AddExport("d3d8", "D3DDevice_SetShaderConstantMode", &D3DDevice_SetShaderConstantMode);
    registry.AddExport("d3d8", "D3DDevice_SetScreenSpaceOffset", &D3DDevice_SetScreenSpaceOffset);
    registry.AddExport("d3d8", "D3DDevice_SetMaterial", &D3DDevice_SetMaterial);
    registry.AddExport("d3d8", "D3DDevice_SetLight", &D3DDevice_SetLight);
    registry.AddExport("d3d8", "D3DDevice_LightEnable", &D3DDevice_LightEnable);
    registry.AddExport("d3d8", "D3DDevice_SetScissors", &AcceptThree);
}

}  // namespace rexbox::hle::d3d8
