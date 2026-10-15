#include "d3d8_gl_targets.h"

#include "d3d8_gl.h"
#include "d3d8_internal.h"
#include "d3d8_resource.h"
#include "d3d8_trace.h"

#include <cstdio>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

struct ColorTarget
{
    GLuint texture = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    // For sampling (SampleRenderTarget): a copy in D3D's row order, the
    // framebuffer that writes it, and whether the target was bound since the
    // copy was made.
    GLuint copy = 0;
    GLuint copy_framebuffer = 0;
    bool bound_since_copy = true;
    // The surface's pixel memory when last bound (physical); a texture over
    // it samples this target until the memory is freed.
    bool has_data = false;
    uint32_t data = 0;
};

struct DepthTarget
{
    GLuint renderbuffer = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

std::unordered_map<GuestAddress, ColorTarget> g_colors;
std::unordered_map<GuestAddress, DepthTarget> g_depths;
std::map<std::pair<GuestAddress, GuestAddress>, GLuint> g_framebuffers;
GLuint g_bound_framebuffer = 0;  // GL_FRAMEBUFFER as BindRenderTarget left it, valid in g_framebuffer_epoch
uint32_t g_framebuffer_epoch = 0;
GuestAddress g_bound_color = 0;
GuestAddress g_bound_depth = 0;
uint32_t g_width = 0;
uint32_t g_height = 0;

// The pair BindRenderTarget resolved last, so a run of draws to one target
// skips the map lookups. DropIfResized clears it when it removes entries.
struct ResolvedTarget
{
    bool valid = false;
    GuestAddress color = 0;
    GuestAddress depth = 0;
    GLuint framebuffer = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};
ResolvedTarget g_resolved_target;

void SurfaceSize(GuestAddress surface, uint32_t* width, uint32_t* height)
{
    const LevelLayout layout = HeaderLevelLayout(surface, 0);
    *width = layout.width;
    *height = layout.height;
}

// A header can be set up again for a surface of another size (the title
// reuses them); GL objects made for the old size are dropped and remade.
// Returns true when anything was dropped.
bool DropIfResized(GuestAddress color, GuestAddress depth)
{
    bool dropped = false;
    uint32_t width = 0;
    uint32_t height = 0;
    GlFunctions& gl = Gl();
    if (color != 0)
    {
        const auto it = g_colors.find(color);
        if (it != g_colors.end())
        {
            SurfaceSize(color, &width, &height);
            if (width != it->second.width || height != it->second.height)
            {
                ColorTarget& old = it->second;
                gl.DeleteTextures(1, &old.texture);
                if (old.copy != 0)
                {
                    gl.DeleteTextures(1, &old.copy);
                    gl.DeleteFramebuffers(1, &old.copy_framebuffer);
                }
                g_colors.erase(it);
                dropped = true;
            }
        }
    }
    if (depth != 0)
    {
        const auto it = g_depths.find(depth);
        if (it != g_depths.end())
        {
            SurfaceSize(depth, &width, &height);
            if (width != it->second.width || height != it->second.height)
            {
                gl.DeleteRenderbuffers(1, &it->second.renderbuffer);
                g_depths.erase(it);
                dropped = true;
            }
        }
    }
    if (dropped)
    {
        for (auto it = g_framebuffers.begin(); it != g_framebuffers.end();)
        {
            if ((color != 0 && it->first.first == color) || (depth != 0 && it->first.second == depth))
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
    return dropped;
}

const ColorTarget& Color(GuestAddress surface)
{
    auto it = g_colors.find(surface);
    if (it != g_colors.end())
    {
        return it->second;
    }
    GlFunctions& gl = Gl();
    ColorTarget target;
    SurfaceSize(surface, &target.width, &target.height);
    InvalidateGlStateCache();  // binds on the active texture unit
    gl.GenTextures(1, &target.texture);
    gl.BindTexture(kGlTexture2D, target.texture);
    gl.TexImage2D(kGlTexture2D, 0, static_cast<GLint>(kGlRgba8), static_cast<GLsizei>(target.width),
                  static_cast<GLsizei>(target.height), 0, kGlRgba, kGlUnsignedByte, nullptr);
    gl.TexParameteri(kGlTexture2D, kGlTextureMinFilter, static_cast<GLint>(kGlLinear));
    gl.TexParameteri(kGlTexture2D, kGlTextureMagFilter, static_cast<GLint>(kGlLinear));
    gl.TexParameteri(kGlTexture2D, kGlTextureMaxLevel, 0);
    const HostResource* host = FindResource(surface);
    if (host == nullptr || (host->kind != ResourceKind::kBackBuffer))
    {
        ReportOnce(Format("target %08X", surface),
                   Format("render target %08X (%ux%u) is not a back buffer: drawn into its own GL texture, "
                          "sampled through a copy",
                          surface, target.width, target.height));
    }
    return g_colors.emplace(surface, target).first->second;
}

const DepthTarget& Depth(GuestAddress surface)
{
    auto it = g_depths.find(surface);
    if (it != g_depths.end())
    {
        return it->second;
    }
    GlFunctions& gl = Gl();
    DepthTarget target;
    SurfaceSize(surface, &target.width, &target.height);
    gl.GenRenderbuffers(1, &target.renderbuffer);
    gl.BindRenderbuffer(kGlRenderbuffer, target.renderbuffer);
    gl.RenderbufferStorage(kGlRenderbuffer, kGlDepth24Stencil8, static_cast<GLsizei>(target.width),
                           static_cast<GLsizei>(target.height));
    return g_depths.emplace(surface, target).first->second;
}

GLuint Framebuffer(GuestAddress color, GuestAddress depth)
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
    if (color != 0)
    {
        gl.FramebufferTexture2D(kGlFramebuffer, kGlColorAttachment0, kGlTexture2D, Color(color).texture, 0);
    }
    if (depth != 0)
    {
        gl.FramebufferRenderbuffer(kGlFramebuffer, kGlDepthStencilAttachment, kGlRenderbuffer,
                                   Depth(depth).renderbuffer);
    }
    const GLenum status = gl.CheckFramebufferStatus(kGlFramebuffer);
    if (status != kGlFramebufferComplete)
    {
        ReportOnce(Format("fbo %08X %08X", color, depth),
                   Format("framebuffer for color %08X depth %08X is incomplete (status %04X)", color, depth, status));
    }
    g_framebuffers.emplace(key, framebuffer);
    return framebuffer;
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
    DropIfResized(color, depth);
    if (!resolved.valid || resolved.color != color || resolved.depth != depth)
    {
        resolved.framebuffer = Framebuffer(color, depth);
        resolved.color = color;
        resolved.depth = depth;
        // Neither surface: the size of the previous target stays.
        resolved.width = g_width;
        resolved.height = g_height;
        if (color != 0)
        {
            resolved.width = Color(color).width;
            resolved.height = Color(color).height;
        }
        else if (depth != 0)
        {
            resolved.width = Depth(depth).width;
            resolved.height = Depth(depth).height;
        }
        resolved.valid = color != 0 || depth != 0;
        if (color != 0)
        {
            ColorTarget& target = g_colors.find(color)->second;  // made by Framebuffer() above
            target.bound_since_copy = true;
            target.has_data = true;
            target.data = Memory().Read32(color + header::kData) & kPhysicalMask;
        }
    }
    // Other paths that bind framebuffers (present, dumps, creation)
    // invalidate the GL state cache.
    if (g_framebuffer_epoch != GlStateEpoch() || g_bound_framebuffer != resolved.framebuffer)
    {
        Gl().BindFramebuffer(kGlFramebuffer, resolved.framebuffer);
        g_bound_framebuffer = resolved.framebuffer;
        g_framebuffer_epoch = GlStateEpoch();
    }
    g_bound_color = color;
    g_bound_depth = depth;
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
    data &= kPhysicalMask;
    for (auto& [address, target] : g_colors)
    {
        if (target.texture == 0 || !target.has_data || target.data != data)
        {
            continue;
        }
        GlFunctions& gl = Gl();
        if (target.copy == 0)
        {
            InvalidateGlStateCache();  // binds a texture and a framebuffer
            gl.GenTextures(1, &target.copy);
            gl.BindTexture(kGlTexture2D, target.copy);
            gl.TexImage2D(kGlTexture2D, 0, static_cast<GLint>(kGlRgba8), static_cast<GLsizei>(target.width),
                          static_cast<GLsizei>(target.height), 0, kGlRgba, kGlUnsignedByte, nullptr);
            gl.TexParameteri(kGlTexture2D, kGlTextureMaxLevel, 0);
            gl.GenFramebuffers(1, &target.copy_framebuffer);
            gl.BindFramebuffer(kGlFramebuffer, target.copy_framebuffer);
            gl.FramebufferTexture2D(kGlFramebuffer, kGlColorAttachment0, kGlTexture2D, target.copy, 0);
            gl.BindFramebuffer(kGlFramebuffer, Framebuffer(g_bound_color, g_bound_depth));
            target.bound_since_copy = true;
        }
        // Draws and clears write only the bound target.
        if (target.bound_since_copy)
        {
            const GLint w = static_cast<GLint>(target.width);
            const GLint h = static_cast<GLint>(target.height);
            gl.BindFramebuffer(kGlReadFramebuffer, Framebuffer(address, 0));
            gl.BindFramebuffer(kGlDrawFramebuffer, target.copy_framebuffer);
            gl.Disable(kGlScissorTest);
            gl.ColorMask(1, 1, 1, 1);
            // GL keeps D3D's top row at the top; textures have it at row 0.
            gl.BlitFramebuffer(0, 0, w, h, 0, h, w, 0, kGlColorBufferBit, kGlNearest);
            gl.BindFramebuffer(kGlFramebuffer, Framebuffer(g_bound_color, g_bound_depth));
            InvalidateGlStateCache();
            // Still bound: later draws go to it, so copy again next time.
            target.bound_since_copy = address == g_bound_color;
        }
        *surface = address;
        *width = target.width;
        *height = target.height;
        return target.copy;
    }
    return 0;
}

void ForgetRenderTargetMemory(GuestAddress data)
{
    data &= kPhysicalMask;
    for (auto& [address, target] : g_colors)
    {
        if (target.has_data && target.data == data)
        {
            target.has_data = false;
        }
    }
}

uint32_t RenderTargetTexture(GuestAddress color)
{
    const auto it = g_colors.find(color);
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
    // Fit with the aspect ratio kept; the rest of the window is black.
    uint32_t width = window_width;
    uint32_t height = source.height * window_width / (source.width != 0 ? source.width : 1);
    if (height > window_height)
    {
        height = window_height;
        width = source.width * window_height / (source.height != 0 ? source.height : 1);
    }
    const GLint x = static_cast<GLint>((window_width - width) / 2);
    const GLint y = static_cast<GLint>((window_height - height) / 2);

    gl.BindFramebuffer(kGlReadFramebuffer, Framebuffer(color, 0));
    gl.BindFramebuffer(kGlDrawFramebuffer, 0);
    gl.Disable(kGlScissorTest);
    gl.ColorMask(1, 1, 1, 1);
    gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(kGlColorBufferBit);
    if (ramp.IsIdentity() || !DrawThroughRamp(source.texture, ramp, x, y, width, height))
    {
        gl.BlitFramebuffer(0, 0, static_cast<GLint>(source.width), static_cast<GLint>(source.height), x, y,
                           x + static_cast<GLint>(width), y + static_cast<GLint>(height), kGlColorBufferBit,
                           kGlLinear);
    }
    platform::PresentDisplay();
    gl.BindFramebuffer(kGlFramebuffer, Framebuffer(g_bound_color, g_bound_depth));
    InvalidateGlStateCache();
}

void ReadRenderTarget(GuestAddress color, std::vector<uint8_t>* rgba, uint32_t* width, uint32_t* height)
{
    GlFunctions& gl = Gl();
    const ColorTarget& source = Color(color);
    rgba->resize(static_cast<size_t>(source.width) * source.height * 4);
    gl.BindFramebuffer(kGlReadFramebuffer, Framebuffer(color, 0));
    gl.PixelStorei(kGlPackAlignment, 1);
    gl.ReadPixels(0, 0, static_cast<GLsizei>(source.width), static_cast<GLsizei>(source.height), kGlRgba,
                  kGlUnsignedByte, rgba->data());
    gl.BindFramebuffer(kGlFramebuffer, Framebuffer(g_bound_color, g_bound_depth));
    InvalidateGlStateCache();
    *width = source.width;
    *height = source.height;
}

bool DumpRenderTarget(GuestAddress color, const std::string& path, const GammaRamp& ramp)
{
    std::vector<uint8_t> rgba;
    uint32_t width = 0;
    uint32_t height = 0;
    ReadRenderTarget(color, &rgba, &width, &height);
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
