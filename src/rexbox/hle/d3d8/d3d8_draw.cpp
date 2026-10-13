// Drawing entry points on GL: the vertices and indices come from guest
// memory (kept in GL buffers while they stay the same, d3d8_gl_buffers.h),
// the fixed-function state comes from the library's globals, and the result
// goes to the bound render target
// (docs/design/20261012-i023-d3d8-fixed-function-draw.md section 3).
// Vertex-shader programs are translated to GLSL (d3d8_vsh.h) and paired with
// the fixed-function fragment stage or a register-combiner pixel shader
// (d3d8_psh.h). GL state is sent only when it differs from what the previous
// draw set (docs/design/20261013-i040-draw-performance.md). The immediate-mode
// Begin/End path is reported once and not drawn yet.

#include "d3d8_ff_shader.h"
#include "d3d8_fvf.h"
#include "d3d8_gl.h"
#include "d3d8_gl_buffers.h"
#include "d3d8_gl_programs.h"
#include "d3d8_gl_targets.h"
#include "d3d8_gl_textures.h"
#include "d3d8_globals.h"
#include "d3d8_internal.h"
#include "d3d8_profile.h"
#include "d3d8_render_state.h"
#include "d3d8_resource.h"
#include "d3d8_psh.h"
#include "d3d8_shader.h"
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
    bool from_buffer = false;  // a vertex buffer (cached) rather than UP data (streamed)
};

// Where a draw's indices come from.
struct IndexSource
{
    GuestAddress address = 0;  // 16-bit guest indices; 0: vertices base .. base + count - 1
    uint32_t base = 0;         // added to each index
    bool cacheable = false;    // index data the title keeps (DrawIndexedVertices)
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
FixedFunctionKey BuildKey(const VertexLayout& layout, BoundTexture textures[kFixedFunctionStages], bool program)
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
            ReportOncePer("texgen", tex_coord_index >> 16, [&] {
                return Format("texture coordinate generation %08X is not implemented yet (stage %u)",
                              tex_coord_index, s);
            });
        }
        if (TextureStateValue(s, tss::kTextureTransformFlags) != 0)
        {
            ReportOncePer("texture transform", 0, [&] {
                return Format("texture transforms (stage %u flags %08X) are not applied yet", s,
                              TextureStateValue(s, tss::kTextureTransformFlags));
            });
        }
        for (const uint32_t op : {stage.color_op, stage.alpha_op})
        {
            if (op == top::kPremodulate || op == top::kBumpEnvMap || op == top::kBumpEnvMapLuminance)
            {
                ReportOncePer("op", op, [op] { return Format("texture operation %u is drawn as SELECTARG1", op); });
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
        ReportOncePer("fog", 0, [] { return std::string("fog is not drawn yet"); });
    }
    if (!program && !layout.pretransformed && RenderStateValue(rs::kLighting) != 0)
    {
        ReportOncePer("lighting", 0, [] {
            return std::string("fixed-function lighting is not implemented yet; lit draws use vertex colors");
        });
    }
    return key;
}

// Fixed per-fragment state from the render-state globals (design section
// 3.6), with the viewport and depth range.
struct FixedState
{
    bool depth_test = false;
    uint32_t depth_func = 0;
    bool depth_write = false;
    bool blend = false;
    uint32_t src_blend = 0;
    uint32_t dest_blend = 0;
    uint32_t blend_op = 0;
    uint32_t blend_color = 0;
    bool cull = false;
    uint32_t front_face = 0;
    uint32_t cull_face = 0;
    bool stencil = false;
    uint32_t stencil_func = 0;
    uint32_t stencil_ref = 0;
    uint32_t stencil_mask = 0;
    uint32_t stencil_fail = 0;
    uint32_t stencil_zfail = 0;
    uint32_t stencil_pass = 0;
    uint32_t stencil_write_mask = 0;
    uint32_t color_write = 0;
    bool polygon_offset = false;
    uint32_t offset_slope = 0;  // float bits
    uint32_t offset_units = 0;
    uint32_t fill = 0;
    int32_t viewport[4] = {};
    float depth_range[2] = {};
};

FixedState g_applied_state;
uint32_t g_applied_epoch = 0;

FixedState CurrentFixedState(GuestAddress depth_target, bool pretransformed, uint32_t width, uint32_t height)
{
    FixedState s;
    s.depth_test = depth_target != 0 && RenderStateValue(rs::kZEnable) != 0;
    if (s.depth_test)
    {
        s.depth_func = RenderStateValue(rs::kZFunc);
    }
    s.depth_write = s.depth_test && RenderStateValue(rs::kZWriteEnable) != 0;
    s.blend = RenderStateValue(rs::kAlphaBlendEnable) != 0;
    if (s.blend)
    {
        s.src_blend = RenderStateValue(rs::kSrcBlend);
        s.dest_blend = RenderStateValue(rs::kDestBlend);
        s.blend_op = RenderStateValue(rs::kBlendOp);
        s.blend_color = RenderStateValue(rs::kBlendColor);
    }
    const uint32_t cull = RenderStateValue(rs::kCullMode);
    s.cull = cull != kCullNone;
    if (s.cull)
    {
        const uint32_t front = RenderStateValue(rs::kFrontFace);
        s.front_face = front == kGlCcw ? kGlCcw : kGlCw;
        s.cull_face = cull == front ? kGlFront : kGlBack;
    }
    s.stencil = depth_target != 0 && RenderStateValue(rs::kStencilEnable) != 0;
    if (s.stencil)
    {
        s.stencil_func = RenderStateValue(rs::kStencilFunc);
        s.stencil_ref = RenderStateValue(rs::kStencilRef);
        s.stencil_mask = RenderStateValue(rs::kStencilMask);
        s.stencil_fail = RenderStateValue(rs::kStencilFail);
        s.stencil_zfail = RenderStateValue(rs::kStencilZFail);
        s.stencil_pass = RenderStateValue(rs::kStencilPass);
        s.stencil_write_mask = RenderStateValue(rs::kStencilWriteMask);
    }
    s.color_write = RenderStateValue(rs::kColorWriteEnable);
    s.polygon_offset = RenderStateValue(rs::kSolidOffsetEnable) != 0;
    if (s.polygon_offset)
    {
        s.offset_slope = RenderStateValue(rs::kPolygonOffsetZSlopeScale);
        s.offset_units = RenderStateValue(rs::kPolygonOffsetZOffset);
    }
    const uint32_t fill = RenderStateValue(rs::kFillMode);
    s.fill = fill == kGlPoint || fill == kGlLine ? fill : kGlFill;
    if (pretransformed)
    {
        // Screen-space vertices are not mapped by the viewport.
        s.viewport[2] = static_cast<int32_t>(width);
        s.viewport[3] = static_cast<int32_t>(height);
        s.depth_range[1] = 1.0f;
    }
    else
    {
        const Viewport& vp = State().viewport;
        s.viewport[0] = static_cast<int32_t>(vp.x);
        s.viewport[1] = static_cast<int32_t>(height) - static_cast<int32_t>(vp.y + vp.height);
        s.viewport[2] = static_cast<int32_t>(vp.width);
        s.viewport[3] = static_cast<int32_t>(vp.height);
        s.depth_range[0] = vp.min_z;
        s.depth_range[1] = vp.max_z;
    }
    return s;
}

void SetCapability(GLenum capability, bool enabled)
{
    if (enabled)
    {
        Gl().Enable(capability);
    }
    else
    {
        Gl().Disable(capability);
    }
}

// Sends the parts of `s` that differ from what the previous draw set (all of
// it after the GL state cache was invalidated).
void ApplyFixedState(const FixedState& s)
{
    GlFunctions& gl = Gl();
    const bool known = g_applied_epoch == GlStateEpoch();
    const FixedState& last = g_applied_state;
    if (!known)
    {
        gl.Disable(kGlScissorTest);
    }
    if (!known || s.depth_test != last.depth_test)
    {
        SetCapability(kGlDepthTest, s.depth_test);
    }
    if (s.depth_test && (!known || s.depth_func != last.depth_func || !last.depth_test))
    {
        gl.DepthFunc(s.depth_func);
    }
    if (!known || s.depth_write != last.depth_write)
    {
        gl.DepthMask(s.depth_write ? 1 : 0);
    }
    if (!known || s.blend != last.blend)
    {
        SetCapability(kGlBlend, s.blend);
    }
    if (s.blend && (!known || !last.blend || s.src_blend != last.src_blend || s.dest_blend != last.dest_blend ||
                    s.blend_op != last.blend_op || s.blend_color != last.blend_color))
    {
        gl.BlendFunc(s.src_blend, s.dest_blend);
        gl.BlendEquation(s.blend_op);
        float color[4];
        ColorToFloats(s.blend_color, color);
        gl.BlendColor(color[0], color[1], color[2], color[3]);
    }
    if (!known || s.cull != last.cull)
    {
        SetCapability(kGlCullFace, s.cull);
    }
    if (s.cull && (!known || !last.cull || s.front_face != last.front_face || s.cull_face != last.cull_face))
    {
        gl.FrontFace(s.front_face);
        gl.CullFace(s.cull_face);
    }
    if (!known || s.stencil != last.stencil)
    {
        SetCapability(kGlStencilTest, s.stencil);
    }
    if (s.stencil &&
        (!known || !last.stencil || s.stencil_func != last.stencil_func || s.stencil_ref != last.stencil_ref ||
         s.stencil_mask != last.stencil_mask || s.stencil_fail != last.stencil_fail ||
         s.stencil_zfail != last.stencil_zfail || s.stencil_pass != last.stencil_pass ||
         s.stencil_write_mask != last.stencil_write_mask))
    {
        gl.StencilFunc(s.stencil_func, static_cast<GLint>(s.stencil_ref), s.stencil_mask);
        gl.StencilOp(s.stencil_fail, s.stencil_zfail, s.stencil_pass);
        gl.StencilMask(s.stencil_write_mask);
    }
    if (!known || s.color_write != last.color_write)
    {
        // D3DCOLORWRITEENABLE: one byte per channel, A R G B from the top (inferred).
        const uint32_t write = s.color_write;
        gl.ColorMask((write >> 16) & 0xFF ? 1 : 0, (write >> 8) & 0xFF ? 1 : 0, write & 0xFF ? 1 : 0,
                     (write >> 24) & 0xFF ? 1 : 0);
    }
    if (!known || s.polygon_offset != last.polygon_offset)
    {
        SetCapability(kGlPolygonOffsetFill, s.polygon_offset);
    }
    if (s.polygon_offset && (!known || !last.polygon_offset || s.offset_slope != last.offset_slope ||
                             s.offset_units != last.offset_units))
    {
        gl.PolygonOffset(FloatBits(s.offset_slope), FloatBits(s.offset_units));
    }
    if (gl.PolygonMode != nullptr && (!known || s.fill != last.fill))
    {
        gl.PolygonMode(kGlFrontAndBack, s.fill);
    }
    if (!known || std::memcmp(s.viewport, last.viewport, sizeof(s.viewport)) != 0)
    {
        gl.Viewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
    }
    if (!known || std::memcmp(s.depth_range, last.depth_range, sizeof(s.depth_range)) != 0)
    {
        GlDepthRange(s.depth_range[0], s.depth_range[1]);
    }
    g_applied_state = s;
    g_applied_epoch = GlStateEpoch();
}

// `base` is the vertex data's offset in the streaming vertex buffer.
void SetVertexAttributes(const VertexLayout& layout, uint32_t stride, size_t base)
{
    GlFunctions& gl = Gl();
    const auto attribute = [&gl, stride, base](uint32_t index, int offset, GLint size, GLenum type,
                                               bool normalized) {
        if (offset == kAbsent)
        {
            gl.DisableVertexAttribArray(index);
            return;
        }
        gl.EnableVertexAttribArray(index);
        gl.VertexAttribPointer(index, size, type, normalized ? 1 : 0, static_cast<GLsizei>(stride),
                               reinterpret_cast<const void*>(base + static_cast<uintptr_t>(offset)));
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

// Vertex attributes v0..v15 of a vertex-shader program from its declaration
// (stream 0, `stride` bytes per vertex). Registers the program reads but the
// declaration leaves out keep GL's default (0, 0, 0, 1).
void SetProgramAttributes(const vsh::Declaration& declaration, uint32_t stride, uint32_t used_inputs, size_t base)
{
    GlFunctions& gl = Gl();
    uint32_t enabled = 0;
    for (const vsh::VertexElement& element : declaration.elements)
    {
        if ((used_inputs & (1u << element.reg)) == 0 || element.stream != 0)
        {
            continue;
        }
        GLenum type = kGlFloat;
        bool normalized = false;
        switch (element.kind)
        {
        case vsh::ElementKind::kFloat:
            break;
        case vsh::ElementKind::kNormShort:
            type = kGlShort;
            normalized = true;
            break;
        case vsh::ElementKind::kShort:
            type = kGlShort;
            break;
        case vsh::ElementKind::kNormByte:
        case vsh::ElementKind::kD3dColor:
            type = kGlUnsignedByte;
            normalized = true;
            break;
        }
        gl.EnableVertexAttribArray(element.reg);
        gl.VertexAttribPointer(element.reg, element.components, type, normalized ? 1 : 0, static_cast<GLsizei>(stride),
                               reinterpret_cast<const void*>(base + static_cast<uintptr_t>(element.offset)));
        enabled |= 1u << element.reg;
    }
    for (uint32_t reg = 0; reg < vsh::kInputs; ++reg)
    {
        if ((enabled & (1u << reg)) == 0)
        {
            gl.DisableVertexAttribArray(reg);
        }
    }
}

// One draw of `count` vertices or indices; vertex numbers are relative to
// `source.base`.
void Draw(const char* name, uint32_t primitive, uint32_t count, IndexSource indices, VertexSource source)
{
    profile::Lap lap;
    const D3dState& state = State();
    if (!state.display_created || count == 0)
    {
        return;
    }
    const PipelineState& pipeline = Pipeline();
    const bool is_program = (pipeline.vertex_shader & fvf::kProgramHandle) != 0;
    const psh::Definition* pixel_shader = nullptr;
    if (pipeline.pixel_shader != 0)
    {
        pixel_shader = FindPixelShader(pipeline.pixel_shader);
        if (!is_program || pixel_shader == nullptr)
        {
            ReportOncePer("pixel shader", 0, [&] {
                return Format("%s with pixel shader %08X and %s is not drawn yet", name, pipeline.pixel_shader,
                              is_program ? "an unknown handle" : "fixed-function vertex processing");
            });
            return;
        }
    }
    const ProgramShader* shader = nullptr;
    VertexLayout layout;
    if (is_program)
    {
        shader = FindProgramShader(pipeline.vertex_shader);
        if (shader == nullptr || !shader->usable)
        {
            ReportOncePer("vertex program", pipeline.vertex_shader, [&] {
                return Format("%s: vertex-shader program %08X is not drawn: %s", name, pipeline.vertex_shader,
                              shader != nullptr ? shader->error.c_str() : "unknown handle");
            });
            return;
        }
        if ((shader->declaration.streams & ~1u) != 0)
        {
            ReportOncePer("vertex program streams", pipeline.vertex_shader, [&] {
                return Format("%s: vertex-shader program %08X reads streams other than 0, not drawn yet", name,
                              pipeline.vertex_shader);
            });
            return;
        }
        // The fragment stage reads every output the program may write.
        layout.diffuse = 0;
        layout.specular = 0;
        layout.tex_coords = kMaxTexCoords;
        for (uint32_t i = 0; i < kMaxTexCoords; ++i)
        {
            layout.tex_components[i] = 4;
        }
        layout.stride = shader->declaration.stream_bytes[0];
    }
    else if (!DecodeFvf(pipeline.vertex_shader, &layout))
    {
        ReportOncePer("fvf", pipeline.vertex_shader, [&] {
            return Format("%s: vertex format %08X is not understood", name, pipeline.vertex_shader);
        });
        return;
    }
    GLenum mode = kGlTriangles;
    if (!GlPrimitive(primitive, &mode))
    {
        ReportOncePer("primitive", primitive,
                      [&] { return Format("%s: primitive type %u is not understood", name, primitive); });
        return;
    }
    if (D3d8TraceEnabled())
    {
        TraceOnce(Format("draw fvf %08X prim %u", pipeline.vertex_shader, primitive),
                  Format("%s: FVF %08X (stride %u), primitive %u", name, pipeline.vertex_shader, layout.stride,
                         primitive));
    }

    GlFunctions& gl = Gl();
    BoundTexture textures[kFixedFunctionStages];
    FixedFunctionKey key;
    psh::Key pixel_key;
    bool sampled[kFixedFunctionStages] = {};
    if (pixel_shader != nullptr)
    {
        // With a pixel shader the texture-stage operations do not apply; the
        // shader's texture modes say which stages sample.
        pixel_key.definition = *pixel_shader;
        for (uint32_t s = 0; s < kFixedFunctionStages; ++s)
        {
            if (psh::TextureMode(*pixel_shader, s) == psh::texmode::kNone)
            {
                continue;
            }
            textures[s] = ResolveTexture(pipeline.textures[s]);
            pixel_key.samplers[s] = textures[s].kind;
            pixel_key.unnormalized[s] = textures[s].unnormalized;
            sampled[s] = textures[s].kind != SamplerKind::kNone;
        }
        if (RenderStateValue(rs::kAlphaTestEnable) != 0)
        {
            pixel_key.alpha_func = RenderStateValue(rs::kAlphaFunc);
        }
    }
    else
    {
        key = BuildKey(layout, textures, is_program);
        for (uint32_t s = 0; s < kFixedFunctionStages; ++s)
        {
            sampled[s] = key.stages[s].sampler != SamplerKind::kNone;
        }
    }
    const FixedFunctionProgram* program = nullptr;
    const VertexProgramGl* vertex_program = nullptr;
    if (is_program)
    {
        vertex_program = pixel_shader != nullptr ? GetVertexPixelProgram(*shader, pixel_key) : GetVertexProgram(*shader, key);
        if (vertex_program == nullptr)
        {
            return;
        }
    }
    else
    {
        program = GetFixedFunctionProgram(key);
        if (program == nullptr)
        {
            return;
        }
    }

    lap.Mark(profile::Phase::kSetup);
    BindRenderTarget(state.render_target, state.depth_target);
    lap.Mark(profile::Phase::kTarget);
    BindDrawVertexArray();
    DrawIndices index_data;
    const bool arrays = indices.address == 0 && primitive != prim::kQuadList;
    if (indices.address != 0)
    {
        if (!PrepareGuestIndices(indices.address, count, indices.base, primitive == prim::kQuadList,
                                 indices.cacheable, &index_data))
        {
            return;
        }
    }
    else if (!arrays)
    {
        // A quad list without indices: two triangles per quad.
        std::vector<uint32_t> triangles;
        triangles.reserve(count / 4 * 6);
        for (uint32_t q = indices.base; q + 3 < indices.base + count; q += 4)
        {
            triangles.insert(triangles.end(), {q, q + 1, q + 2, q, q + 2, q + 3});
        }
        if (!PrepareHostIndices(triangles, &index_data))
        {
            return;
        }
    }
    else
    {
        index_data.first = indices.base;
        index_data.vertex_count = count;
        index_data.count = count;
    }
    lap.Mark(profile::Phase::kIndices);
    const uint32_t stride = source.stride != 0 ? source.stride : layout.stride;
    const GuestAddress begin = (source.base + index_data.first * stride) & kPhysicalMask;
    const uint32_t bytes = index_data.vertex_count * stride;
    if (!Memory().IsValid(begin, bytes))
    {
        ReportOnce(Format("vertices %08X", begin),
                   Format("%s: vertices %08X+%X are outside guest memory", name, begin, bytes));
        return;
    }
    const size_t vertex_offset = PrepareVertexData(begin, bytes, source.from_buffer);
    lap.Mark(profile::Phase::kUpload);
    if (is_program)
    {
        SetProgramAttributes(shader->declaration, stride, vertex_program->used_inputs, vertex_offset);
    }
    else
    {
        SetVertexAttributes(layout, stride, vertex_offset);
    }
    lap.Mark(profile::Phase::kAttributes);

    const uint32_t width = RenderTargetWidth();
    const uint32_t height = RenderTargetHeight();
    const int* tex_scale = nullptr;
    int tfactor_location = -1;
    int alpha_ref_location = -1;
    UniformCache* uniforms = nullptr;
    if (is_program)
    {
        UseProgram(vertex_program->program);
        uniforms = &vertex_program->uniforms;
        uniforms->SetArray4(vertex_program->constants, VertexShaderConstants());
        if (pixel_shader != nullptr)
        {
            float c0[psh::kStages][4];
            float c1[psh::kStages][4];
            float final_c0[4];
            float final_c1[4];
            psh::StageConstants(*pixel_shader, PixelShaderConstants(), c0, c1, final_c0, final_c1);
            uniforms->Set4fv(vertex_program->stage_c0, static_cast<int>(psh::kStages), &c0[0][0]);
            uniforms->Set4fv(vertex_program->stage_c1, static_cast<int>(psh::kStages), &c1[0][0]);
            uniforms->Set4fv(vertex_program->final_c0, 1, final_c0);
            uniforms->Set4fv(vertex_program->final_c1, 1, final_c1);
            float fog[4];
            ColorToFloats(RenderStateValue(rs::kFogColor), fog);
            uniforms->Set4fv(vertex_program->fog_color, 1, fog);
        }
        tex_scale = vertex_program->tex_scale;
        tfactor_location = vertex_program->tfactor;
        alpha_ref_location = vertex_program->alpha_ref;
    }
    else
    {
        UseProgram(program->program);
        uniforms = &program->uniforms;
        uniforms->Set4f(program->target, static_cast<float>(width), static_cast<float>(height),
                        pipeline.screen_space_offset_x, pipeline.screen_space_offset_y);
        const std::array<float, 16> transform =
            Multiply(Multiply(pipeline.transforms[ts::kWorld], pipeline.transforms[ts::kView]),
                     pipeline.transforms[ts::kProjection]);
        uniforms->SetMatrix4(program->transform, transform.data());
        tex_scale = program->tex_scale;
        tfactor_location = program->tfactor;
        alpha_ref_location = program->alpha_ref;
    }
    float tfactor[4];
    ColorToFloats(RenderStateValue(rs::kTextureFactor), tfactor);
    uniforms->Set4fv(tfactor_location, 1, tfactor);
    uniforms->Set1f(alpha_ref_location, static_cast<float>(RenderStateValue(rs::kAlphaRef) & 0xFF));
    lap.Mark(profile::Phase::kConstants);
    for (uint32_t s = 0; s < kFixedFunctionStages; ++s)
    {
        if (!sampled[s])
        {
            continue;
        }
        BindStageTexture(s, textures[s]);
        if (tex_scale[s] >= 0)
        {
            uniforms->Set2f(tex_scale[s], 1.0f / static_cast<float>(std::max(textures[s].width, 1u)),
                            1.0f / static_cast<float>(std::max(textures[s].height, 1u)));
        }
    }

    lap.Mark(profile::Phase::kTextures);
    ApplyFixedState(CurrentFixedState(state.depth_target, layout.pretransformed, width, height));
    lap.Mark(profile::Phase::kState);
    if (arrays)
    {
        gl.DrawArrays(mode, 0, static_cast<GLsizei>(count));
    }
    else
    {
        gl.DrawElements(mode, static_cast<GLsizei>(index_data.count),
                        index_data.use_short ? kGlUnsignedShort : kGlUnsignedInt,
                        reinterpret_cast<const void*>(index_data.offset));
    }
    lap.Mark(profile::Phase::kDraw);
    profile::CountDraw(is_program);
}

VertexSource StreamZero()
{
    VertexSource source;
    const GuestAddress buffer = StreamVertexBuffer(0);
    source.base = buffer != 0 ? Memory().Read32(buffer + header::kData) : 0;
    source.stride = StreamStride(0);
    source.from_buffer = buffer != 0;
    return source;
}

// void D3DDevice_DrawVertices(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT VertexCount)
void D3DDevice_DrawVertices()
{
    HleCall call;
    IndexSource indices;
    indices.base = call.Arg(1);
    Draw("DrawVertices", call.Arg(0), call.Arg(2), indices, StreamZero());
    call.Return(kD3dOk, 3);
}

// void D3DDevice_DrawIndexedVertices(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount, const WORD* pIndexData)
// The indices count from the BaseVertexIndex given to SetIndices.
void D3DDevice_DrawIndexedVertices()
{
    HleCall call;
    IndexSource indices;
    indices.address = call.Arg(2);
    indices.base = Pipeline().base_vertex_index;
    indices.cacheable = true;
    Draw("DrawIndexedVertices", call.Arg(0), call.Arg(1), indices, StreamZero());
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
    Draw("DrawVerticesUP", call.Arg(0), call.Arg(1), IndexSource{}, source);
    call.Return(kD3dOk, 4);
}

// void D3DDevice_DrawIndexedVerticesUP(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount,
//     const WORD* pIndexData, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
void D3DDevice_DrawIndexedVerticesUP()
{
    HleCall call;
    IndexSource indices;
    indices.address = call.Arg(2);
    VertexSource source;
    source.base = call.Arg(3);
    source.stride = call.Arg(4);
    Draw("DrawIndexedVerticesUP", call.Arg(0), call.Arg(1), indices, source);
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
    ReportOncePer("copyrects", 0, [&call] {
        return Format("D3DDevice_CopyRects(%08X, %08X, %u, %08X, %08X) is not implemented yet", call.Arg(0),
                      call.Arg(1), call.Arg(2), call.Arg(3), call.Arg(4));
    });
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
