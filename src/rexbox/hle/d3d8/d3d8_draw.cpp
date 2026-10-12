// Drawing entry points on GL: the vertices and indices are read from guest
// memory on every draw, the fixed-function state comes from the library's
// globals, and the result goes to the bound render target
// (docs/design/20261012-i023-d3d8-fixed-function-draw.md section 3).
// Programmable vertex shaders, register-combiner pixel shaders and the
// immediate-mode Begin/End path are reported once and not drawn yet.

#include "d3d8_ff_shader.h"
#include "d3d8_fvf.h"
#include "d3d8_gl.h"
#include "d3d8_gl_programs.h"
#include "d3d8_gl_targets.h"
#include "d3d8_gl_textures.h"
#include "d3d8_globals.h"
#include "d3d8_internal.h"
#include "d3d8_render_state.h"
#include "d3d8_resource.h"
#include "d3d8_state.h"
#include "d3d8_trace.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

// D3DPRIMITIVETYPE (Xbox; inferred, the title's 5 draws a triangle list).
namespace prim
{
constexpr uint32_t kPointList = 1;
constexpr uint32_t kLineList = 2;
constexpr uint32_t kLineLoop = 3;
constexpr uint32_t kLineStrip = 4;
constexpr uint32_t kTriangleList = 5;
constexpr uint32_t kTriangleStrip = 6;
constexpr uint32_t kTriangleFan = 7;
constexpr uint32_t kQuadList = 8;
constexpr uint32_t kQuadStrip = 9;
constexpr uint32_t kPolygon = 10;
}  // namespace prim

// D3DZBUFFERTYPE, D3DCULL (Xbox values are the GL face enums), fill modes.
constexpr uint32_t kCullNone = 0;

struct VertexSource
{
    GuestAddress base = 0;  // address of vertex 0
    uint32_t stride = 0;
};

float FloatBits(uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, 4);
    return value;
}

void ColorToFloats(uint32_t argb, float out[4])
{
    out[0] = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
    out[1] = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
    out[2] = static_cast<float>(argb & 0xFF) / 255.0f;
    out[3] = static_cast<float>(argb >> 24) / 255.0f;
}

// Row-major 4x4 product a * b (D3D's row-vector convention).
std::array<float, 16> Multiply(const std::array<float, 16>& a, const std::array<float, 16>& b)
{
    std::array<float, 16> result = {};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                sum += a[row * 4 + k] * b[k * 4 + column];
            }
            result[row * 4 + column] = sum;
        }
    }
    return result;
}

// The GL primitive for an Xbox one; QUADLIST is expanded by the caller.
bool GlPrimitive(uint32_t primitive, GLenum* mode)
{
    switch (primitive)
    {
    case prim::kPointList:
        *mode = kGlPoints;
        return true;
    case prim::kLineList:
        *mode = kGlLines;
        return true;
    case prim::kLineLoop:
        *mode = kGlLineLoop;
        return true;
    case prim::kLineStrip:
        *mode = kGlLineStrip;
        return true;
    case prim::kTriangleList:
    case prim::kQuadList:
        *mode = kGlTriangles;
        return true;
    case prim::kTriangleStrip:
    case prim::kQuadStrip:
        *mode = kGlTriangleStrip;
        return true;
    case prim::kTriangleFan:
    case prim::kPolygon:
        *mode = kGlTriangleFan;
        return true;
    default:
        return false;
    }
}

// Builds the fixed-function key from the vertex layout and the stage state;
// binds each active stage's texture. Reports what this batch leaves out.
FixedFunctionKey BuildKey(const VertexLayout& layout, BoundTexture textures[kFixedFunctionStages])
{
    const PipelineState& pipeline = Pipeline();
    FixedFunctionKey key;
    key.pretransformed = layout.pretransformed;
    key.has_diffuse = layout.diffuse != kAbsent;
    key.has_specular = layout.specular != kAbsent;
    key.tex_coords = static_cast<uint8_t>(layout.tex_coords);
    for (uint32_t i = 0; i < kMaxTexCoords; ++i)
    {
        key.tex_components[i] = static_cast<uint8_t>(layout.tex_components[i]);
    }
    for (uint32_t s = 0; s < kFixedFunctionStages; ++s)
    {
        FixedFunctionKey::Stage& stage = key.stages[s];
        const uint32_t color_op = TextureStateValue(s, tss::kColorOp);
        stage.color_op = color_op == 0 ? top::kDisable : color_op;
        if (stage.color_op == top::kDisable)
        {
            break;
        }
        stage.color_arg[0] = TextureStateValue(s, tss::kColorArg0);
        stage.color_arg[1] = TextureStateValue(s, tss::kColorArg1);
        stage.color_arg[2] = TextureStateValue(s, tss::kColorArg2);
        stage.alpha_op = TextureStateValue(s, tss::kAlphaOp);
        stage.alpha_arg[0] = TextureStateValue(s, tss::kAlphaArg0);
        stage.alpha_arg[1] = TextureStateValue(s, tss::kAlphaArg1);
        stage.alpha_arg[2] = TextureStateValue(s, tss::kAlphaArg2);
        stage.result_arg = TextureStateValue(s, tss::kResultArg);
        const uint32_t tex_coord_index = TextureStateValue(s, tss::kTexCoordIndex);
        stage.tex_coord_index = static_cast<uint8_t>(tex_coord_index & 0xFFFF);
        if ((tex_coord_index & 0xFFFF0000u) != 0)
        {
            ReportOnce(Format("texgen %X", tex_coord_index >> 16),
                       Format("texture coordinate generation %08X is not implemented yet (stage %u)",
                              tex_coord_index, s));
        }
        if (TextureStateValue(s, tss::kTextureTransformFlags) != 0)
        {
            ReportOnce("texture transform", Format("texture transforms (stage %u flags %08X) are not applied yet", s,
                                                   TextureStateValue(s, tss::kTextureTransformFlags)));
        }
        for (const uint32_t op : {stage.color_op, stage.alpha_op})
        {
            if (op == top::kPremodulate || op == top::kBumpEnvMap || op == top::kBumpEnvMapLuminance)
            {
                ReportOnce(Format("op %u", op), Format("texture operation %u is drawn as SELECTARG1", op));
            }
        }
        textures[s] = ResolveTexture(pipeline.textures[s]);
        stage.sampler = textures[s].kind;
        stage.unnormalized = textures[s].unnormalized;
    }
    key.specular_enable = RenderStateValue(rs::kSpecularEnable) != 0;
    if (RenderStateValue(rs::kAlphaTestEnable) != 0)
    {
        key.alpha_func = RenderStateValue(rs::kAlphaFunc);
    }
    if (RenderStateValue(rs::kFogEnable) != 0)
    {
        ReportOnce("fog", "fog is not drawn yet");
    }
    if (!layout.pretransformed && RenderStateValue(rs::kLighting) != 0)
    {
        ReportOnce("lighting", "fixed-function lighting is not implemented yet; lit draws use vertex colors");
    }
    return key;
}

// Fixed per-fragment state from the render-state globals (design section 3.6).
void ApplyFixedState(GuestAddress depth_target)
{
    GlFunctions& gl = Gl();
    gl.Disable(kGlScissorTest);

    const bool depth = depth_target != 0 && RenderStateValue(rs::kZEnable) != 0;
    if (depth)
    {
        gl.Enable(kGlDepthTest);
        gl.DepthFunc(RenderStateValue(rs::kZFunc));
    }
    else
    {
        gl.Disable(kGlDepthTest);
    }
    gl.DepthMask(depth && RenderStateValue(rs::kZWriteEnable) != 0 ? 1 : 0);

    if (RenderStateValue(rs::kAlphaBlendEnable) != 0)
    {
        gl.Enable(kGlBlend);
        gl.BlendFunc(RenderStateValue(rs::kSrcBlend), RenderStateValue(rs::kDestBlend));
        gl.BlendEquation(RenderStateValue(rs::kBlendOp));
        float color[4];
        ColorToFloats(RenderStateValue(rs::kBlendColor), color);
        gl.BlendColor(color[0], color[1], color[2], color[3]);
    }
    else
    {
        gl.Disable(kGlBlend);
    }

    const uint32_t cull = RenderStateValue(rs::kCullMode);
    if (cull == kCullNone)
    {
        gl.Disable(kGlCullFace);
    }
    else
    {
        const uint32_t front = RenderStateValue(rs::kFrontFace);
        gl.Enable(kGlCullFace);
        gl.FrontFace(front == kGlCcw ? kGlCcw : kGlCw);
        gl.CullFace(cull == front ? kGlFront : kGlBack);
    }

    if (depth_target != 0 && RenderStateValue(rs::kStencilEnable) != 0)
    {
        gl.Enable(kGlStencilTest);
        gl.StencilFunc(RenderStateValue(rs::kStencilFunc), static_cast<GLint>(RenderStateValue(rs::kStencilRef)),
                       RenderStateValue(rs::kStencilMask));
        gl.StencilOp(RenderStateValue(rs::kStencilFail), RenderStateValue(rs::kStencilZFail),
                     RenderStateValue(rs::kStencilPass));
        gl.StencilMask(RenderStateValue(rs::kStencilWriteMask));
    }
    else
    {
        gl.Disable(kGlStencilTest);
    }

    // D3DCOLORWRITEENABLE: one byte per channel, A R G B from the top (inferred).
    const uint32_t write = RenderStateValue(rs::kColorWriteEnable);
    gl.ColorMask((write >> 16) & 0xFF ? 1 : 0, (write >> 8) & 0xFF ? 1 : 0, write & 0xFF ? 1 : 0,
                 (write >> 24) & 0xFF ? 1 : 0);

    if (RenderStateValue(rs::kSolidOffsetEnable) != 0)
    {
        gl.Enable(kGlPolygonOffsetFill);
        gl.PolygonOffset(FloatBits(RenderStateValue(rs::kPolygonOffsetZSlopeScale)),
                         FloatBits(RenderStateValue(rs::kPolygonOffsetZOffset)));
    }
    else
    {
        gl.Disable(kGlPolygonOffsetFill);
    }

    if (gl.PolygonMode != nullptr)
    {
        const uint32_t fill = RenderStateValue(rs::kFillMode);
        gl.PolygonMode(kGlFrontAndBack, fill == kGlPoint || fill == kGlLine ? fill : kGlFill);
    }
}

void SetVertexAttributes(const VertexLayout& layout, uint32_t stride)
{
    GlFunctions& gl = Gl();
    const auto attribute = [&gl, stride](uint32_t index, int offset, GLint size, GLenum type, bool normalized) {
        if (offset == kAbsent)
        {
            gl.DisableVertexAttribArray(index);
            return;
        }
        gl.EnableVertexAttribArray(index);
        gl.VertexAttribPointer(index, size, type, normalized ? 1 : 0, static_cast<GLsizei>(stride),
                               reinterpret_cast<const void*>(static_cast<uintptr_t>(offset)));
    };
    attribute(attr::kPosition, 0, static_cast<GLint>(layout.position_components), kGlFloat, false);
    attribute(attr::kDiffuse, layout.diffuse, 4, kGlUnsignedByte, true);
    attribute(attr::kSpecular, layout.specular, 4, kGlUnsignedByte, true);
    for (uint32_t i = 0; i < kMaxTexCoords; ++i)
    {
        attribute(attr::kTexCoord0 + i, layout.tex_offset[i], static_cast<GLint>(layout.tex_components[i]), kGlFloat,
                  false);
    }
}

// One draw: `indices` are vertex numbers relative to `source.base` (implicit
// 0..count-1 when empty).
void Draw(const char* name, uint32_t primitive, uint32_t count, std::vector<uint32_t> indices, VertexSource source)
{
    const D3dState& state = State();
    if (!state.display_created || count == 0)
    {
        return;
    }
    const PipelineState& pipeline = Pipeline();
    if ((pipeline.vertex_shader & fvf::kProgramHandle) != 0)
    {
        ReportOnce("vertex program", Format("%s with a vertex-shader program (%08X) is not drawn yet", name,
                                            pipeline.vertex_shader));
        return;
    }
    if (pipeline.pixel_shader != 0)
    {
        ReportOnce("pixel shader", Format("%s with pixel shader %08X (register combiners) is not drawn yet", name,
                                          pipeline.pixel_shader));
        return;
    }
    VertexLayout layout;
    if (!DecodeFvf(pipeline.vertex_shader, &layout))
    {
        ReportOnce(Format("fvf %08X", pipeline.vertex_shader),
                   Format("%s: vertex format %08X is not understood", name, pipeline.vertex_shader));
        return;
    }
    GLenum mode = kGlTriangles;
    if (!GlPrimitive(primitive, &mode))
    {
        ReportOnce(Format("primitive %u", primitive), Format("%s: primitive type %u is not understood", name, primitive));
        return;
    }
    TraceOnce(Format("draw fvf %08X prim %u", pipeline.vertex_shader, primitive),
              Format("%s: FVF %08X (stride %u), primitive %u", name, pipeline.vertex_shader, layout.stride, primitive));

    if (indices.empty())
    {
        indices.resize(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            indices[i] = i;
        }
    }
    if (primitive == prim::kQuadList)
    {
        std::vector<uint32_t> triangles;
        triangles.reserve(indices.size() / 4 * 6);
        for (size_t q = 0; q + 3 < indices.size(); q += 4)
        {
            triangles.insert(triangles.end(),
                             {indices[q], indices[q + 1], indices[q + 2], indices[q], indices[q + 2], indices[q + 3]});
        }
        indices.swap(triangles);
    }
    if (indices.empty())
    {
        return;
    }
    const auto [low, high] = std::minmax_element(indices.begin(), indices.end());
    const uint32_t first = *low;
    const uint32_t vertex_count = *high - first + 1;
    const uint32_t stride = source.stride != 0 ? source.stride : layout.stride;
    const GuestAddress begin = (source.base + first * stride) & kPhysicalMask;
    const uint32_t bytes = vertex_count * stride;
    runtime::GuestMemory& memory = Memory();
    if (!memory.IsValid(begin, bytes))
    {
        ReportOnce(Format("vertices %08X", begin),
                   Format("%s: vertices %08X+%X are outside guest memory", name, begin, bytes));
        return;
    }

    GlFunctions& gl = Gl();
    BoundTexture textures[kFixedFunctionStages];
    const FixedFunctionKey key = BuildKey(layout, textures);
    const FixedFunctionProgram* program = GetFixedFunctionProgram(key);
    if (program == nullptr)
    {
        return;
    }

    BindRenderTarget(state.render_target, state.depth_target);
    BindStreamingBuffers();
    UploadVertexData(memory.Translate(begin), bytes);
    SetVertexAttributes(layout, stride);
    std::vector<uint16_t> short_indices;
    std::vector<uint32_t> long_indices;
    const bool use_short = vertex_count <= 0x10000;
    if (use_short)
    {
        short_indices.reserve(indices.size());
        for (const uint32_t index : indices)
        {
            short_indices.push_back(static_cast<uint16_t>(index - first));
        }
        UploadIndexData(short_indices.data(), short_indices.size() * 2);
    }
    else
    {
        for (const uint32_t index : indices)
        {
            long_indices.push_back(index - first);
        }
        UploadIndexData(long_indices.data(), long_indices.size() * 4);
    }

    gl.UseProgram(program->program);
    const uint32_t width = RenderTargetWidth();
    const uint32_t height = RenderTargetHeight();
    gl.Uniform4f(program->target, static_cast<float>(width), static_cast<float>(height),
                 pipeline.screen_space_offset_x, pipeline.screen_space_offset_y);
    const std::array<float, 16> transform =
        Multiply(Multiply(pipeline.transforms[ts::kWorld], pipeline.transforms[ts::kView]),
                 pipeline.transforms[ts::kProjection]);
    gl.UniformMatrix4fv(program->transform, 1, 0, transform.data());
    float tfactor[4];
    ColorToFloats(RenderStateValue(rs::kTextureFactor), tfactor);
    gl.Uniform4f(program->tfactor, tfactor[0], tfactor[1], tfactor[2], tfactor[3]);
    gl.Uniform1f(program->alpha_ref, static_cast<float>(RenderStateValue(rs::kAlphaRef) & 0xFF));
    for (uint32_t s = 0; s < kFixedFunctionStages; ++s)
    {
        if (key.stages[s].sampler == SamplerKind::kNone)
        {
            continue;
        }
        BindStageTexture(s, textures[s]);
        if (program->tex_scale[s] >= 0)
        {
            gl.Uniform2f(program->tex_scale[s], 1.0f / static_cast<float>(std::max(textures[s].width, 1u)),
                         1.0f / static_cast<float>(std::max(textures[s].height, 1u)));
        }
    }

    ApplyFixedState(state.depth_target);
    if (layout.pretransformed)
    {
        // Screen-space vertices are not mapped by the viewport.
        gl.Viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        GlDepthRange(0.0f, 1.0f);
    }
    else
    {
        const Viewport& vp = state.viewport;
        gl.Viewport(static_cast<GLint>(vp.x), static_cast<GLint>(height) - static_cast<GLint>(vp.y + vp.height),
                    static_cast<GLsizei>(vp.width), static_cast<GLsizei>(vp.height));
        GlDepthRange(vp.min_z, vp.max_z);
    }
    gl.DrawElements(mode, static_cast<GLsizei>(indices.size()), use_short ? kGlUnsignedShort : kGlUnsignedInt,
                    nullptr);
}

std::vector<uint32_t> ReadIndices(GuestAddress address, uint32_t count, uint32_t base)
{
    std::vector<uint32_t> indices;
    runtime::GuestMemory& memory = Memory();
    address &= kPhysicalMask;
    if (!memory.IsValid(address, count * 2))
    {
        return indices;
    }
    indices.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        indices.push_back(memory.Read16(address + i * 2) + base);
    }
    return indices;
}

VertexSource StreamZero()
{
    VertexSource source;
    const GuestAddress buffer = StreamVertexBuffer(0);
    source.base = buffer != 0 ? Memory().Read32(buffer + header::kData) : 0;
    source.stride = StreamStride(0);
    return source;
}

// void D3DDevice_DrawVertices(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT VertexCount)
void D3DDevice_DrawVertices()
{
    HleCall call;
    const uint32_t start = call.Arg(1);
    const uint32_t count = call.Arg(2);
    std::vector<uint32_t> indices(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        indices[i] = start + i;
    }
    Draw("DrawVertices", call.Arg(0), count, std::move(indices), StreamZero());
    call.Return(kD3dOk, 3);
}

// void D3DDevice_DrawIndexedVertices(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount, const WORD* pIndexData)
// The indices count from the BaseVertexIndex given to SetIndices.
void D3DDevice_DrawIndexedVertices()
{
    HleCall call;
    const uint32_t count = call.Arg(1);
    std::vector<uint32_t> indices = ReadIndices(call.Arg(2), count, Pipeline().base_vertex_index);
    if (indices.size() == count)
    {
        Draw("DrawIndexedVertices", call.Arg(0), count, std::move(indices), StreamZero());
    }
    call.Return(kD3dOk, 3);
}

// void D3DDevice_DrawVerticesUP(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount,
//     const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
void D3DDevice_DrawVerticesUP()
{
    HleCall call;
    VertexSource source;
    source.base = call.Arg(2);
    source.stride = call.Arg(3);
    Draw("DrawVerticesUP", call.Arg(0), call.Arg(1), {}, source);
    call.Return(kD3dOk, 4);
}

// void D3DDevice_DrawIndexedVerticesUP(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount,
//     const WORD* pIndexData, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
void D3DDevice_DrawIndexedVerticesUP()
{
    HleCall call;
    const uint32_t count = call.Arg(1);
    std::vector<uint32_t> indices = ReadIndices(call.Arg(2), count, 0);
    VertexSource source;
    source.base = call.Arg(3);
    source.stride = call.Arg(4);
    if (indices.size() == count)
    {
        Draw("DrawIndexedVerticesUP", call.Arg(0), count, std::move(indices), source);
    }
    call.Return(kD3dOk, 5);
}

// void D3DDevice_Begin(D3DPRIMITIVETYPE PrimitiveType) and the immediate-mode
// vertex setters: not used by the title so far.
void D3DDevice_Begin()
{
    HleCall call;
    ReportOnce("begin", "D3DDevice_Begin/End immediate-mode drawing is not implemented yet");
    call.Return(kD3dOk, 1);
}

// void D3DDevice_End(void)
void D3DDevice_End()
{
    HleCall call;
    call.Return(kD3dOk, 0);
}

// void D3DDevice_SetVertexData2f(int Register, float a, float b)
void D3DDevice_SetVertexData2f()
{
    HleCall call;
    call.Return(kD3dOk, 3);
}

// void D3DDevice_SetVertexData4f(int Register, float a, float b, float c, float d)
void D3DDevice_SetVertexData4f()
{
    HleCall call;
    call.Return(kD3dOk, 5);
}

// HRESULT D3DDevice_CopyRects(D3DSurface* pSourceSurface, const RECT* pSourceRectsArray, UINT cRects,
//     D3DSurface* pDestinationSurface, const POINT* pDestPointsArray)
void D3DDevice_CopyRects()
{
    HleCall call;
    ReportOnce("copyrects", Format("D3DDevice_CopyRects(%08X, %08X, %u, %08X, %08X) is not implemented yet",
                                   call.Arg(0), call.Arg(1), call.Arg(2), call.Arg(3), call.Arg(4)));
    call.Return(kD3dOk, 5);
}

}  // namespace

void RegisterDrawExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "D3DDevice_DrawVertices", &D3DDevice_DrawVertices);
    registry.AddExport("d3d8", "D3DDevice_DrawIndexedVertices", &D3DDevice_DrawIndexedVertices);
    registry.AddExport("d3d8", "D3DDevice_DrawVerticesUP", &D3DDevice_DrawVerticesUP);
    registry.AddExport("d3d8", "D3DDevice_DrawIndexedVerticesUP", &D3DDevice_DrawIndexedVerticesUP);
    registry.AddExport("d3d8", "D3DDevice_Begin", &D3DDevice_Begin);
    registry.AddExport("d3d8", "D3DDevice_End", &D3DDevice_End);
    registry.AddExport("d3d8", "D3DDevice_SetVertexData2f", &D3DDevice_SetVertexData2f);
    registry.AddExport("d3d8", "D3DDevice_SetVertexData4f", &D3DDevice_SetVertexData4f);
    registry.AddExport("d3d8", "D3DDevice_CopyRects", &D3DDevice_CopyRects);
}

}  // namespace rexbox::hle::d3d8
