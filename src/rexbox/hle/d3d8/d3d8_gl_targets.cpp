#include "d3d8_gl_targets.h"

#include "d3d8_display_state.h"
#include "d3d8_gl.h"
#include "d3d8_internal.h"
#include "d3d8_resource.h"
#include "d3d8_smaa.h"
#include "d3d8_trace.h"
#include "d3d8_vblank.h"

#include "rexbox/platform/overlay.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

// A surface is its pixel memory: the title points a header at other memory
// again (one header alternates between two surfaces of different sizes), so
// the GL objects are keyed by the physical data address, not by the header
// (docs/design/20261017-i071-render-target-by-memory.md).
constexpr uint32_t kNoSurface = 0xFFFFFFFFu;

uint32_t SurfaceMemory(GuestAddress surface)
{
    return surface != 0 ? Memory().Read32(surface + header::kData) & kPhysicalMask : kNoSurface;
}

// The internal rendering scale: GL storage, viewports, scissors and blits
// are this many times the guest's sizes, which D3D keeps seeing
// (docs/design/20261017-i080-render-scale.md). A change waits for the end of
// a present so no frame mixes sizes.
constexpr uint32_t kMaxRenderScale = 8;

uint32_t InitialRenderScale()
{
    const char* setting = std::getenv("REXBOX_RENDER_SCALE");
    if (setting == nullptr || *setting == '\0')
    {
        // The application's settings (docs/design/20261017-i084-settings-overlay.md).
        return std::clamp<uint32_t>(CurrentDisplayDefaults().render_scale, 1, kMaxRenderScale);
    }
    const long value = std::strtol(setting, nullptr, 10);
    const uint32_t scale = static_cast<uint32_t>(std::clamp<long>(value, 1, kMaxRenderScale));
    if (value != static_cast<long>(scale))
    {
        ReportOnce("render scale range", Format("REXBOX_RENDER_SCALE=%s is outside 1..%u; using %u", setting,
                                                kMaxRenderScale, scale));
    }
    return scale;
}

uint32_t g_render_scale = 0;  // REXBOX_RENDER_SCALE, read on first use
uint32_t g_pending_render_scale = 0;  // 0: no change waiting

uint32_t ActiveScale()
{
    if (g_render_scale == 0)
    {
        g_render_scale = InitialRenderScale();
        if (g_render_scale != 1)
        {
            std::fprintf(stderr, "[d3d8] render scale %u\n", g_render_scale);
        }
    }
    return g_render_scale;
}

// MSAA: render targets draw into multisample renderbuffers and are resolved
// into their textures when read (docs/design/20261017-i089-anti-aliasing.md).
// 0 is off; a change waits for the end of a present, like the scale.
uint32_t g_msaa_samples = 0xFFFFFFFFu;  // read from the defaults on first use
uint32_t g_pending_msaa_samples = 0xFFFFFFFFu;  // none waiting

uint32_t ClampSamples(uint32_t samples)
{
    const uint32_t limit = static_cast<uint32_t>(GlMaxSamples());
    if (samples < 2 || limit < 2)
    {
        return 0;
    }
    uint32_t result = 2;
    while (result * 2 <= samples && result * 2 <= limit)
    {
        result *= 2;
    }
    return result;
}

uint32_t ActiveSamples()
{
    if (g_msaa_samples == 0xFFFFFFFFu)
    {
        g_msaa_samples = ClampSamples(CurrentDisplayDefaults().msaa_samples);
        if (g_msaa_samples != 0)
        {
            std::fprintf(stderr, "[d3d8] MSAA %ux\n", g_msaa_samples);
        }
    }
    return g_msaa_samples;
}

struct ColorTarget
{
    GLuint texture = 0;
    uint32_t width = 0;  // guest size; the GL texture is scale times this
    uint32_t height = 0;
    uint32_t scale = 1;
    // MSAA: the multisample renderbuffer drawn into, the framebuffer holding
    // `texture` that it resolves to, and whether it was drawn since.
    uint32_t samples = 0;
    GLuint msaa = 0;
    GLuint resolve_framebuffer = 0;
    bool unresolved = false;
    // The header last bound over this memory, for reports.
    GuestAddress header = 0;
    // For sampling (SampleRenderTarget): a copy in D3D's row order, the
    // framebuffer that writes it, and whether the target was bound since the
    // copy was made.
    GLuint copy = 0;
    GLuint copy_framebuffer = 0;
    bool bound_since_copy = true;
    // Cleared when the memory is freed, set again when it is bound.
    bool sampleable = false;
};

struct DepthTarget
{
    GLuint renderbuffer = 0;
    uint32_t width = 0;  // guest size
    uint32_t height = 0;
    uint32_t scale = 1;
    uint32_t samples = 0;
};

// Keyed by surface memory (SurfaceMemory); framebuffers by the memory pair.
std::unordered_map<uint32_t, ColorTarget> g_colors;
std::unordered_map<uint32_t, DepthTarget> g_depths;
std::map<std::pair<uint32_t, uint32_t>, GLuint> g_framebuffers;
GLuint g_bound_framebuffer = 0;  // GL_FRAMEBUFFER as BindRenderTarget left it, valid in g_framebuffer_epoch
uint32_t g_framebuffer_epoch = 0;
uint32_t g_bound_color = kNoSurface;  // memory of the bound surfaces
uint32_t g_bound_depth = kNoSurface;
uint32_t g_width = 0;
uint32_t g_height = 0;

// What BindRenderTarget resolved last, so a run of draws to one target skips
// the map lookups. The header words are compared too: a header pointed at
// other memory or set up for another size resolves again.
struct ResolvedTarget
{
    bool valid = false;
    GuestAddress color = 0;
    GuestAddress depth = 0;
    uint32_t color_words[3] = {};  // format, size, data
    uint32_t depth_words[3] = {};
    GLuint framebuffer = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};
ResolvedTarget g_resolved_target;

void HeaderWords(GuestAddress surface, uint32_t* words)
{
    if (surface == 0)
    {
        words[0] = words[1] = words[2] = 0;
        return;
    }
    runtime::GuestMemory& memory = Memory();
    words[0] = memory.Read32(surface + header::kFormat);
    words[1] = memory.Read32(surface + header::kSize);
    words[2] = memory.Read32(surface + header::kData);
}

void SurfaceSize(GuestAddress surface, uint32_t* width, uint32_t* height)
{
    const LevelLayout layout = HeaderLevelLayout(surface, 0);
    *width = layout.width;
    *height = layout.height;
}

// Deletes the framebuffers that use color memory `color` or depth memory
// `depth` (kNoSurface matches none).
void DropFramebuffers(uint32_t color, uint32_t depth)
{
    GlFunctions& gl = Gl();
    for (auto it = g_framebuffers.begin(); it != g_framebuffers.end();)
    {
        if ((color != kNoSurface && it->first.first == color) || (depth != kNoSurface && it->first.second == depth))
        {
            gl.DeleteFramebuffers(1, &it->second);
            it = g_framebuffers.erase(it);
        }
        else
        {
            ++it;
        }
    }
    g_resolved_target.valid = false;
    InvalidateGlStateCache();
}

// The color target at `surface`'s memory, made (or remade, when a surface of
// another size came to the same memory) as needed.
void ResolveColor(uint32_t memory, ColorTarget& target);

ColorTarget& Color(GuestAddress surface)
{
    const uint32_t memory = SurfaceMemory(surface);
    uint32_t width = 0;
    uint32_t height = 0;
    SurfaceSize(surface, &width, &height);
    GlFunctions& gl = Gl();
    // Only the scale changed: the old pixels are stretched into the new
    // texture, since a surface may be sampled before it is drawn again.
    GLuint rescaled = 0;
    uint32_t rescaled_width = 0;
    uint32_t rescaled_height = 0;
    bool sampleable = false;
    auto it = g_colors.find(memory);
    if (it != g_colors.end())
    {
        ColorTarget& old = it->second;
        if (old.width == width && old.height == height && old.scale == ActiveScale() &&
            old.samples == ActiveSamples())
        {
            return old;
        }
        if (old.width == width && old.height == height)
        {
            ResolveColor(memory, old);  // the multisample pixels into the texture first
            rescaled = old.texture;
            rescaled_width = old.width * old.scale;
            rescaled_height = old.height * old.scale;
            sampleable = old.sampleable;
        }
        else
        {
            gl.DeleteTextures(1, &old.texture);
        }
        if (old.copy != 0)
        {
            gl.DeleteTextures(1, &old.copy);
            gl.DeleteFramebuffers(1, &old.copy_framebuffer);
        }
        if (old.msaa != 0)
        {
            gl.DeleteRenderbuffers(1, &old.msaa);
        }
        if (old.resolve_framebuffer != 0)
        {
            gl.DeleteFramebuffers(1, &old.resolve_framebuffer);
        }
        g_colors.erase(it);
        DropFramebuffers(memory, kNoSurface);
    }
    ColorTarget target;
    target.width = width;
    target.height = height;
    target.scale = ActiveScale();
    target.samples = ActiveSamples();
    target.header = surface;
    target.sampleable = sampleable;
    InvalidateGlStateCache();  // binds on the active texture unit
    gl.GenTextures(1, &target.texture);
    gl.BindTexture(kGlTexture2D, target.texture);
    gl.TexImage2D(kGlTexture2D, 0, static_cast<GLint>(kGlRgba8), static_cast<GLsizei>(target.width * target.scale),
                  static_cast<GLsizei>(target.height * target.scale), 0, kGlRgba, kGlUnsignedByte, nullptr);
    gl.TexParameteri(kGlTexture2D, kGlTextureMinFilter, static_cast<GLint>(kGlLinear));
    gl.TexParameteri(kGlTexture2D, kGlTextureMagFilter, static_cast<GLint>(kGlLinear));
    gl.TexParameteri(kGlTexture2D, kGlTextureMaxLevel, 0);
    if (target.samples != 0)
    {
        gl.GenRenderbuffers(1, &target.msaa);
        gl.BindRenderbuffer(kGlRenderbuffer, target.msaa);
        gl.RenderbufferStorageMultisample(kGlRenderbuffer, static_cast<GLsizei>(target.samples), kGlRgba8,
                                          static_cast<GLsizei>(target.width * target.scale),
                                          static_cast<GLsizei>(target.height * target.scale));
        gl.GenFramebuffers(1, &target.resolve_framebuffer);
        gl.BindFramebuffer(kGlFramebuffer, target.resolve_framebuffer);
        gl.FramebufferTexture2D(kGlFramebuffer, kGlColorAttachment0, kGlTexture2D, target.texture, 0);
        gl.BindFramebuffer(kGlFramebuffer, 0);
    }
    const HostResource* host = FindResource(surface);
    if (host == nullptr || (host->kind != ResourceKind::kBackBuffer))
    {
        ReportOnce(Format("target %08X %ux%u", memory, target.width, target.height),
                   Format("render target %08X at %08X (%ux%u) is not a back buffer: drawn into its own GL texture, "
                          "sampled through a copy",
                          surface, memory, target.width, target.height));
    }
    if (rescaled != 0)
    {
        GLuint framebuffers[2] = {};
        gl.GenFramebuffers(2, framebuffers);
        gl.BindFramebuffer(kGlReadFramebuffer, framebuffers[0]);
        gl.FramebufferTexture2D(kGlReadFramebuffer, kGlColorAttachment0, kGlTexture2D, rescaled, 0);
        gl.BindFramebuffer(kGlDrawFramebuffer, framebuffers[1]);
        gl.FramebufferTexture2D(kGlDrawFramebuffer, kGlColorAttachment0, kGlTexture2D, target.texture, 0);
        gl.Disable(kGlScissorTest);
        gl.ColorMask(1, 1, 1, 1);
        gl.BlitFramebuffer(0, 0, static_cast<GLint>(rescaled_width), static_cast<GLint>(rescaled_height), 0, 0,
                           static_cast<GLint>(target.width * target.scale),
                           static_cast<GLint>(target.height * target.scale), kGlColorBufferBit, kGlLinear);
        gl.BindFramebuffer(kGlFramebuffer, 0);
        gl.DeleteFramebuffers(2, framebuffers);
        gl.DeleteTextures(1, &rescaled);
        InvalidateGlStateCache();  // callers bind their framebuffer again
    }
    return g_colors.emplace(memory, target).first->second;
}

const DepthTarget& Depth(GuestAddress surface)
{
    const uint32_t memory = SurfaceMemory(surface);
    uint32_t width = 0;
    uint32_t height = 0;
    SurfaceSize(surface, &width, &height);
    GlFunctions& gl = Gl();
    auto it = g_depths.find(memory);
    if (it != g_depths.end())
    {
        // Depth is cleared each frame; a new scale needs no copy.
        if (it->second.width == width && it->second.height == height && it->second.scale == ActiveScale() &&
            it->second.samples == ActiveSamples())
        {
            return it->second;
        }
        gl.DeleteRenderbuffers(1, &it->second.renderbuffer);
        g_depths.erase(it);
        DropFramebuffers(kNoSurface, memory);
    }
    DepthTarget target;
    target.width = width;
    target.height = height;
    target.scale = ActiveScale();
    target.samples = ActiveSamples();
    gl.GenRenderbuffers(1, &target.renderbuffer);
    gl.BindRenderbuffer(kGlRenderbuffer, target.renderbuffer);
    if (target.samples != 0)
    {
        gl.RenderbufferStorageMultisample(kGlRenderbuffer, static_cast<GLsizei>(target.samples), kGlDepth24Stencil8,
                                          static_cast<GLsizei>(target.width * target.scale),
                                          static_cast<GLsizei>(target.height * target.scale));
    }
    else
    {
        gl.RenderbufferStorage(kGlRenderbuffer, kGlDepth24Stencil8, static_cast<GLsizei>(target.width * target.scale),
                               static_cast<GLsizei>(target.height * target.scale));
    }
    return g_depths.emplace(memory, target).first->second;
}

// The framebuffer for the targets at these memories (kNoSurface: none).
GLuint FramebufferAt(uint32_t color, uint32_t depth)
{
    const auto key = std::make_pair(color, depth);
    auto it = g_framebuffers.find(key);
    if (it != g_framebuffers.end())
    {
        return it->second;
    }
    GlFunctions& gl = Gl();
    GLuint framebuffer = 0;
    gl.GenFramebuffers(1, &framebuffer);
    InvalidateGlStateCache();  // binds the new framebuffer
    gl.BindFramebuffer(kGlFramebuffer, framebuffer);
    const auto color_it = g_colors.find(color);
    if (color_it != g_colors.end() && color_it->second.msaa != 0)
    {
        gl.FramebufferRenderbuffer(kGlFramebuffer, kGlColorAttachment0, kGlRenderbuffer, color_it->second.msaa);
    }
    else if (color_it != g_colors.end())
    {
        gl.FramebufferTexture2D(kGlFramebuffer, kGlColorAttachment0, kGlTexture2D, color_it->second.texture, 0);
    }
    const auto depth_it = g_depths.find(depth);
    if (depth_it != g_depths.end())
    {
        gl.FramebufferRenderbuffer(kGlFramebuffer, kGlDepthStencilAttachment, kGlRenderbuffer,
                                   depth_it->second.renderbuffer);
    }
    const GLenum status = gl.CheckFramebufferStatus(kGlFramebuffer);
    if (status != kGlFramebufferComplete)
    {
        ReportOnce(Format("fbo %08X %08X", color, depth),
                   Format("framebuffer for color memory %08X depth memory %08X is incomplete (status %04X)", color,
                          depth, status));
        if (ActiveSamples() != 0)
        {
            // The driver refused the multisample buffers (memory, say): MSAA
            // goes off from the next frame.
            ReportOnce("msaa off", Format("MSAA %ux turned off: a multisample framebuffer is incomplete",
                                          ActiveSamples()));
            g_pending_msaa_samples = 0;
        }
    }
    g_framebuffers.emplace(key, framebuffer);
    return framebuffer;
}

// The framebuffer for these surface headers (either may be 0), making or
// remaking their targets first.
GLuint Framebuffer(GuestAddress color, GuestAddress depth)
{
    if (color != 0)
    {
        Color(color);
    }
    if (depth != 0)
    {
        Depth(depth);
    }
    return FramebufferAt(SurfaceMemory(color), SurfaceMemory(depth));
}

// Binds the framebuffer of the current render target again.
void RebindCurrentTarget()
{
    Gl().BindFramebuffer(kGlFramebuffer, FramebufferAt(g_bound_color, g_bound_depth));
}

// Brings a multisample target's pixels into its texture if it was drawn since
// the last resolve (a target still bound may be drawn again, so it stays
// unresolved). Leaves the framebuffer bindings to the caller.
void ResolveColor(uint32_t memory, ColorTarget& target)
{
    if (target.samples == 0 || !target.unresolved)
    {
        return;
    }
    GlFunctions& gl = Gl();
    const GLint w = static_cast<GLint>(target.width * target.scale);
    const GLint h = static_cast<GLint>(target.height * target.scale);
    gl.BindFramebuffer(kGlReadFramebuffer, FramebufferAt(memory, kNoSurface));
    gl.BindFramebuffer(kGlDrawFramebuffer, target.resolve_framebuffer);
    gl.Disable(kGlScissorTest);
    gl.ColorMask(1, 1, 1, 1);
    gl.BlitFramebuffer(0, 0, w, h, 0, 0, w, h, kGlColorBufferBit, kGlNearest);
    InvalidateGlStateCache();
    target.unresolved = memory == g_bound_color;
}

// A framebuffer whose color attachment is `target`'s texture, resolved: what
// copies, present and reads take pixels from.
GLuint ReadableFramebuffer(uint32_t memory, ColorTarget& target)
{
    if (target.samples != 0)
    {
        ResolveColor(memory, target);
        return target.resolve_framebuffer;
    }
    return FramebufferAt(memory, kNoSurface);
}

struct RampPresenter
{
    bool tried = false;
    GLuint program = 0;
    GLuint vertex_array = 0;
    GLuint ramp_texture = 0;
};

RampPresenter g_ramp_presenter;

GLuint CompileStage(GLenum type, const std::string& source)
{
    GlFunctions& gl = Gl();
    const GLuint shader = gl.CreateShader(type);
    const GLchar* text = source.c_str();
    gl.ShaderSource(shader, 1, &text, nullptr);
    gl.CompileShader(shader);
    GLint ok = 0;
    gl.GetShaderiv(shader, kGlCompileStatus, &ok);
    if (ok == 0)
    {
        GLchar log[1024] = {};
        gl.GetShaderInfoLog(shader, sizeof(log), nullptr, log);
        ReportOnce("gamma shader", Format("gamma present shader does not compile: %s", log));
        gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

// Builds the gamma present program once; false (reported) when it cannot.
bool PrepareRampPresenter()
{
    RampPresenter& p = g_ramp_presenter;
    if (p.tried)
    {
        return p.program != 0;
    }
    p.tried = true;
    GlFunctions& gl = Gl();
    const bool es = GlIsEs();
    const GLuint vertex = CompileStage(kGlVertexShader, GammaPresentVertexShader(es));
    const GLuint fragment = CompileStage(kGlFragmentShader, GammaPresentFragmentShader(es));
    if (vertex == 0 || fragment == 0)
    {
        return false;
    }
    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, vertex);
    gl.AttachShader(program, fragment);
    gl.LinkProgram(program);
    gl.DeleteShader(vertex);
    gl.DeleteShader(fragment);
    GLint linked = 0;
    gl.GetProgramiv(program, kGlLinkStatus, &linked);
    if (linked == 0)
    {
        ReportOnce("gamma link", "gamma present program does not link");
        return false;
    }
    gl.UseProgram(program);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_image"), 0);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_ramp"), 1);
    gl.GenVertexArrays(1, &p.vertex_array);
    gl.GenTextures(1, &p.ramp_texture);
    gl.BindTexture(kGlTexture2D, p.ramp_texture);
    gl.TexParameteri(kGlTexture2D, kGlTextureMinFilter, static_cast<GLint>(kGlNearest));
    gl.TexParameteri(kGlTexture2D, kGlTextureMagFilter, static_cast<GLint>(kGlNearest));
    gl.TexParameteri(kGlTexture2D, kGlTextureMaxLevel, 0);
    p.program = program;
    return true;
}

// Draws `texture` into the bound draw framebuffer's (x, y, width, height)
// with each channel looked up in `ramp`. False when the program is missing.
bool DrawThroughRamp(GLuint texture, const GammaRamp& ramp, GLint x, GLint y, uint32_t width, uint32_t height)
{
    if (!PrepareRampPresenter())
    {
        return false;
    }
    GlFunctions& gl = Gl();
    const RampPresenter& p = g_ramp_presenter;
    const auto texels = GammaRampTexels(ramp);
    gl.ActiveTexture(kGlTexture0 + 1);
    gl.BindSampler(1, 0);
    gl.BindTexture(kGlTexture2D, p.ramp_texture);
    gl.PixelStorei(kGlUnpackAlignment, 1);
    gl.TexImage2D(kGlTexture2D, 0, static_cast<GLint>(kGlRgba8), static_cast<GLsizei>(GammaRamp::kEntries), 1, 0,
                  kGlRgba, kGlUnsignedByte, texels.data());
    gl.ActiveTexture(kGlTexture0);
    gl.BindSampler(0, 0);
    gl.BindTexture(kGlTexture2D, texture);
    gl.Disable(kGlDepthTest);
    gl.Disable(kGlBlend);
    gl.Disable(kGlCullFace);
    gl.Disable(kGlStencilTest);
    gl.Disable(kGlPolygonOffsetFill);
    if (gl.PolygonMode != nullptr)
    {
        gl.PolygonMode(kGlFrontAndBack, kGlFill);
    }
    gl.Viewport(x, y, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    gl.UseProgram(p.program);
    gl.BindVertexArray(p.vertex_array);
    gl.DrawArrays(kGlTriangles, 0, 3);
    return true;
}

void WriteLittleEndian(std::vector<uint8_t>* out, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
    {
        out->push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}

}  // namespace

bool BindRenderTarget(GuestAddress color, GuestAddress depth)
{
    ResolvedTarget& resolved = g_resolved_target;
    uint32_t color_words[3];
    uint32_t depth_words[3];
    HeaderWords(color, color_words);
    HeaderWords(depth, depth_words);
    if (!resolved.valid || resolved.color != color || resolved.depth != depth ||
        !std::equal(color_words, color_words + 3, resolved.color_words) ||
        !std::equal(depth_words, depth_words + 3, resolved.depth_words))
    {
        // Neither surface: the size of the previous target stays.
        uint32_t width = g_width;
        uint32_t height = g_height;
        if (depth != 0)
        {
            const DepthTarget& target = Depth(depth);
            width = target.width;
            height = target.height;
        }
        if (color != 0)
        {
            ColorTarget& target = Color(color);
            target.header = color;
            target.bound_since_copy = true;
            target.unresolved = true;
            target.sampleable = true;
            width = target.width;
            height = target.height;
        }
        // Remaking a target above drops framebuffers and clears `resolved`.
        resolved.framebuffer = FramebufferAt(SurfaceMemory(color), SurfaceMemory(depth));
        resolved.color = color;
        resolved.depth = depth;
        std::copy(color_words, color_words + 3, resolved.color_words);
        std::copy(depth_words, depth_words + 3, resolved.depth_words);
        resolved.width = width;
        resolved.height = height;
        resolved.valid = color != 0 || depth != 0;
    }
    // Other paths that bind framebuffers (present, dumps, creation)
    // invalidate the GL state cache.
    if (g_framebuffer_epoch != GlStateEpoch() || g_bound_framebuffer != resolved.framebuffer)
    {
        Gl().BindFramebuffer(kGlFramebuffer, resolved.framebuffer);
        g_bound_framebuffer = resolved.framebuffer;
        g_framebuffer_epoch = GlStateEpoch();
    }
    g_bound_color = SurfaceMemory(color);
    g_bound_depth = SurfaceMemory(depth);
    g_width = resolved.width;
    g_height = resolved.height;
    return true;
}

uint32_t RenderTargetWidth()
{
    return g_width;
}

uint32_t RenderTargetHeight()
{
    return g_height;
}

GLuint SampleRenderTarget(GuestAddress data, GuestAddress* surface, uint32_t* width, uint32_t* height)
{
    const uint32_t memory = data & kPhysicalMask;
    const auto it = g_colors.find(memory);
    if (it == g_colors.end() || it->second.texture == 0 || !it->second.sampleable)
    {
        return 0;
    }
    ColorTarget& target = it->second;
    GlFunctions& gl = Gl();
    if (target.copy == 0)
    {
        InvalidateGlStateCache();  // binds a texture and a framebuffer
        gl.GenTextures(1, &target.copy);
        gl.BindTexture(kGlTexture2D, target.copy);
        gl.TexImage2D(kGlTexture2D, 0, static_cast<GLint>(kGlRgba8), static_cast<GLsizei>(target.width * target.scale),
                      static_cast<GLsizei>(target.height * target.scale), 0, kGlRgba, kGlUnsignedByte, nullptr);
        gl.TexParameteri(kGlTexture2D, kGlTextureMaxLevel, 0);
        gl.GenFramebuffers(1, &target.copy_framebuffer);
        gl.BindFramebuffer(kGlFramebuffer, target.copy_framebuffer);
        gl.FramebufferTexture2D(kGlFramebuffer, kGlColorAttachment0, kGlTexture2D, target.copy, 0);
        RebindCurrentTarget();
        target.bound_since_copy = true;
    }
    // Draws and clears write only the bound target.
    if (target.bound_since_copy)
    {
        const GLint w = static_cast<GLint>(target.width * target.scale);
        const GLint h = static_cast<GLint>(target.height * target.scale);
        gl.BindFramebuffer(kGlReadFramebuffer, ReadableFramebuffer(memory, target));
        gl.BindFramebuffer(kGlDrawFramebuffer, target.copy_framebuffer);
        gl.Disable(kGlScissorTest);
        gl.ColorMask(1, 1, 1, 1);
        // GL keeps D3D's top row at the top; textures have it at row 0.
        gl.BlitFramebuffer(0, 0, w, h, 0, h, w, 0, kGlColorBufferBit, kGlNearest);
        RebindCurrentTarget();
        InvalidateGlStateCache();
        // Still bound: later draws go to it, so copy again next time.
        target.bound_since_copy = memory == g_bound_color;
    }
    *surface = target.header;
    *width = target.width;
    *height = target.height;
    return target.copy;
}

void ForgetRenderTargetMemory(GuestAddress data)
{
    const auto it = g_colors.find(data & kPhysicalMask);
    if (it != g_colors.end())
    {
        it->second.sampleable = false;
    }
}

uint32_t RenderTargetTexture(GuestAddress color)
{
    const auto it = g_colors.find(SurfaceMemory(color));
    return it != g_colors.end() ? it->second.texture : 0;
}

void PresentRenderTarget(GuestAddress color, const GammaRamp& ramp)
{
    GlFunctions& gl = Gl();
    const ColorTarget& source = Color(color);
    platform::DisplayInfo info;
    platform::GetDisplayInfo(&info);
    const uint32_t window_width = info.drawable_width;
    const uint32_t window_height = info.drawable_height;
    // Fit with the picture's aspect ratio kept (16:9 when widescreen
    // stretches it); the rest of the window is black.
    const uint32_t picture_width = Widescreen() ? (source.height * 16 + 8) / 9 : source.width;
    uint32_t width = window_width;
    uint32_t height = source.height * window_width / (picture_width != 0 ? picture_width : 1);
    if (height > window_height)
    {
        height = window_height;
        width = picture_width * window_height / (source.height != 0 ? source.height : 1);
    }
    const GLint x = static_cast<GLint>((window_width - width) / 2);
    const GLint y = static_cast<GLint>((window_height - height) / 2);

    // The picture: the back buffer resolved, then SMAA on it when chosen
    // (at the render-scale size, before it is stretched to the window).
    GLuint picture_framebuffer = ReadableFramebuffer(SurfaceMemory(color), Color(color));
    GLuint picture_texture = source.texture;
    if (Smaa() == SmaaMode::k1x)
    {
        const SmaaOutput smaa = ApplySmaa(source.texture, source.width * source.scale, source.height * source.scale);
        if (smaa.texture != 0)
        {
            picture_framebuffer = smaa.framebuffer;
            picture_texture = smaa.texture;
        }
        InvalidateGlStateCache();
    }
    gl.BindFramebuffer(kGlReadFramebuffer, picture_framebuffer);
    gl.BindFramebuffer(kGlDrawFramebuffer, 0);
    gl.Disable(kGlScissorTest);
    gl.ColorMask(1, 1, 1, 1);
    gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(kGlColorBufferBit);
    if (ramp.IsIdentity() || !DrawThroughRamp(picture_texture, ramp, x, y, width, height))
    {
        gl.BlitFramebuffer(0, 0, static_cast<GLint>(source.width * source.scale),
                           static_cast<GLint>(source.height * source.scale), x, y, x + static_cast<GLint>(width),
                           y + static_cast<GLint>(height), kGlColorBufferBit, kGlLinear);
    }
    // The settings overlay goes over the picture, into the window's
    // framebuffer (still bound for drawing).
    platform::RenderOverlay();
    const auto swap_start = std::chrono::steady_clock::now();
    platform::PresentDisplay();
    const auto swap_end = std::chrono::steady_clock::now();
    // A swap that waited for the host's blank returned at it: on a monitor
    // near the guest refresh the virtual clock follows that phase, so no
    // swap ever waits a blank more. A swap that did not wait (vsync ignored,
    // frames queued) says nothing about the blank and is not followed
    // (docs/design/20261017-i082-vsync.md).
    if (info.swap_interval != 0 && info.refresh_hz > 0 &&
        std::fabs(info.refresh_hz - static_cast<double>(VerticalBlankRate())) <= 1.0 &&
        swap_end - swap_start >= std::chrono::milliseconds(1))
    {
        AlignVerticalBlank(swap_end);
        ReportOnce("vblank aligned", Format("vertical blank clock follows the host's (monitor %.2f Hz)",
                                            info.refresh_hz));
    }
    RebindCurrentTarget();
    InvalidateGlStateCache();
    // Between frames: a requested scale takes effect (targets are remade as
    // they are next used).
    const uint32_t requested = platform::ConsumeRenderScaleRequest();
    if (requested != 0)
    {
        SetRenderScale(requested);
    }
    if (g_pending_msaa_samples != 0xFFFFFFFFu && g_pending_msaa_samples != ActiveSamples())
    {
        g_msaa_samples = g_pending_msaa_samples;
        g_resolved_target.valid = false;
        InvalidateGlStateCache();
        std::fprintf(stderr, "[d3d8] MSAA %s\n", g_msaa_samples != 0 ? Format("%ux", g_msaa_samples).c_str() : "off");
    }
    g_pending_msaa_samples = 0xFFFFFFFFu;
    if (g_pending_render_scale != 0 && g_pending_render_scale != ActiveScale())
    {
        g_render_scale = g_pending_render_scale;
        g_resolved_target.valid = false;
        InvalidateGlStateCache();
        std::fprintf(stderr, "[d3d8] render scale %u\n", g_render_scale);
    }
    g_pending_render_scale = 0;
}

namespace
{

// `color`'s pixels at its guest size (scaled down with nearest sampling, so
// every value is one the target holds) or at its GL size.
void ReadPixelsAt(GuestAddress color, bool guest_size, std::vector<uint8_t>* rgba, uint32_t* width,
                  uint32_t* height)
{
    GlFunctions& gl = Gl();
    ColorTarget& source = Color(color);
    const GLint physical_width = static_cast<GLint>(source.width * source.scale);
    const GLint physical_height = static_cast<GLint>(source.height * source.scale);
    Framebuffer(color, 0);  // made if new
    const GLuint framebuffer = ReadableFramebuffer(SurfaceMemory(color), source);
    const bool shrink = guest_size && source.scale != 1;
    *width = guest_size ? source.width : static_cast<uint32_t>(physical_width);
    *height = guest_size ? source.height : static_cast<uint32_t>(physical_height);
    rgba->resize(static_cast<size_t>(*width) * *height * 4);
    GLuint scratch_texture = 0;
    GLuint scratch_framebuffer = 0;
    if (shrink)
    {
        gl.GenTextures(1, &scratch_texture);
        gl.BindTexture(kGlTexture2D, scratch_texture);
        gl.TexImage2D(kGlTexture2D, 0, static_cast<GLint>(kGlRgba8), static_cast<GLsizei>(*width),
                      static_cast<GLsizei>(*height), 0, kGlRgba, kGlUnsignedByte, nullptr);
        gl.GenFramebuffers(1, &scratch_framebuffer);
        gl.BindFramebuffer(kGlDrawFramebuffer, scratch_framebuffer);
        gl.FramebufferTexture2D(kGlDrawFramebuffer, kGlColorAttachment0, kGlTexture2D, scratch_texture, 0);
        gl.BindFramebuffer(kGlReadFramebuffer, framebuffer);
        gl.Disable(kGlScissorTest);
        gl.BlitFramebuffer(0, 0, physical_width, physical_height, 0, 0, static_cast<GLint>(*width),
                           static_cast<GLint>(*height), kGlColorBufferBit, kGlNearest);
        gl.BindFramebuffer(kGlReadFramebuffer, scratch_framebuffer);
    }
    else
    {
        gl.BindFramebuffer(kGlReadFramebuffer, framebuffer);
    }
    gl.PixelStorei(kGlPackAlignment, 1);
    gl.ReadPixels(0, 0, static_cast<GLsizei>(*width), static_cast<GLsizei>(*height), kGlRgba, kGlUnsignedByte,
                  rgba->data());
    if (shrink)
    {
        gl.BindFramebuffer(kGlFramebuffer, 0);
        gl.DeleteFramebuffers(1, &scratch_framebuffer);
        gl.DeleteTextures(1, &scratch_texture);
    }
    RebindCurrentTarget();
    InvalidateGlStateCache();
}

}  // namespace

void ReadRenderTarget(GuestAddress color, std::vector<uint8_t>* rgba, uint32_t* width, uint32_t* height)
{
    ReadPixelsAt(color, true, rgba, width, height);
}

uint32_t RenderScale()
{
    return ActiveScale();
}

RenderTargetUse RenderTargetBytes()
{
    RenderTargetUse use;
    for (const auto& [memory, target] : g_colors)
    {
        const uint64_t bytes = static_cast<uint64_t>(target.width * target.scale) * (target.height * target.scale) * 4;
        ++use.count;
        use.bytes += bytes * ((target.copy != 0 ? 2 : 1) + target.samples);
    }
    for (const auto& [memory, target] : g_depths)
    {
        ++use.count;
        use.bytes += static_cast<uint64_t>(target.width * target.scale) * (target.height * target.scale) * 4 *
                     std::max<uint32_t>(target.samples, 1);
    }
    return use;
}

void SetRenderScale(uint32_t scale)
{
    g_pending_render_scale = std::clamp<uint32_t>(scale, 1, kMaxRenderScale);
}

uint32_t MsaaSamples()
{
    return ActiveSamples();
}

void SetMsaaSamples(uint32_t samples)
{
    g_pending_msaa_samples = ClampSamples(samples);
}

uint32_t MaxMsaaSamples()
{
    return static_cast<uint32_t>(GlMaxSamples());
}

bool DumpRenderTarget(GuestAddress color, const std::string& path, const GammaRamp& ramp)
{
    std::vector<uint8_t> rgba;
    uint32_t width = 0;
    uint32_t height = 0;
    ReadPixelsAt(color, false, &rgba, &width, &height);
    ApplyGammaRamp(ramp, rgba.data(), static_cast<size_t>(width) * height);
    return WriteBmp(path, rgba.data(), width, height);
}

bool WriteBmp(const std::string& path, const uint8_t* rgba, uint32_t width, uint32_t height)
{
    // 24-bit BMP rows are stored bottom-up like GL's, padded to 4 bytes, as B, G, R.
    const uint32_t row_bytes = (width * 3 + 3) & ~3u;
    const uint32_t image_bytes = row_bytes * height;
    std::vector<uint8_t> file;
    file.push_back('B');
    file.push_back('M');
    WriteLittleEndian(&file, 54 + image_bytes, 4);
    WriteLittleEndian(&file, 0, 4);
    WriteLittleEndian(&file, 54, 4);
    WriteLittleEndian(&file, 40, 4);
    WriteLittleEndian(&file, width, 4);
    WriteLittleEndian(&file, height, 4);
    WriteLittleEndian(&file, 1, 2);
    WriteLittleEndian(&file, 24, 2);
    WriteLittleEndian(&file, 0, 4);
    WriteLittleEndian(&file, image_bytes, 4);
    WriteLittleEndian(&file, 2835, 4);
    WriteLittleEndian(&file, 2835, 4);
    WriteLittleEndian(&file, 0, 4);
    WriteLittleEndian(&file, 0, 4);
    for (uint32_t y = 0; y < height; ++y)
    {
        const uint8_t* row = rgba + static_cast<size_t>(y) * width * 4;
        for (uint32_t x = 0; x < width; ++x)
        {
            file.push_back(row[x * 4 + 2]);
            file.push_back(row[x * 4 + 1]);
            file.push_back(row[x * 4 + 0]);
        }
        for (uint32_t pad = width * 3; pad < row_bytes; ++pad)
        {
            file.push_back(0);
        }
    }
    FILE* out = std::fopen(path.c_str(), "wb");
    if (out == nullptr)
    {
        return false;
    }
    const bool written = std::fwrite(file.data(), 1, file.size(), out) == file.size();
    std::fclose(out);
    return written;
}

}  // namespace rexbox::hle::d3d8
