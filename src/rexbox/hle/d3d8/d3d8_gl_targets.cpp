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
GuestAddress g_bound_color = 0;
GuestAddress g_bound_depth = 0;
uint32_t g_width = 0;
uint32_t g_height = 0;

void SurfaceSize(GuestAddress surface, uint32_t* width, uint32_t* height)
{
    const LevelLayout layout = HeaderLevelLayout(surface, 0);
    *width = layout.width;
    *height = layout.height;
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
                   Format("render target %08X (%ux%u) is not a back buffer: drawn into its own GL texture, not yet "
                          "visible to texture sampling",
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
    GlFunctions& gl = Gl();
    gl.BindFramebuffer(kGlFramebuffer, Framebuffer(color, depth));
    g_bound_color = color;
    g_bound_depth = depth;
    if (color != 0)
    {
        g_width = Color(color).width;
        g_height = Color(color).height;
    }
    else if (depth != 0)
    {
        g_width = Depth(depth).width;
        g_height = Depth(depth).height;
    }
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

bool DumpRenderTarget(GuestAddress color, const std::string& path, const GammaRamp& ramp)
{
    GlFunctions& gl = Gl();
    const ColorTarget& source = Color(color);
    std::vector<uint8_t> rgba(static_cast<size_t>(source.width) * source.height * 4);
    gl.BindFramebuffer(kGlReadFramebuffer, Framebuffer(color, 0));
    gl.PixelStorei(kGlPackAlignment, 1);
    gl.ReadPixels(0, 0, static_cast<GLsizei>(source.width), static_cast<GLsizei>(source.height), kGlRgba,
                  kGlUnsignedByte, rgba.data());
    gl.BindFramebuffer(kGlFramebuffer, Framebuffer(g_bound_color, g_bound_depth));

    ApplyGammaRamp(ramp, rgba.data(), static_cast<size_t>(source.width) * source.height);

    // 24-bit BMP rows are stored bottom-up like GL's, padded to 4 bytes, as B, G, R.
    const uint32_t row_bytes = (source.width * 3 + 3) & ~3u;
    const uint32_t image_bytes = row_bytes * source.height;
    std::vector<uint8_t> file;
    file.push_back('B');
    file.push_back('M');
    WriteLittleEndian(&file, 54 + image_bytes, 4);
    WriteLittleEndian(&file, 0, 4);
    WriteLittleEndian(&file, 54, 4);
    WriteLittleEndian(&file, 40, 4);
    WriteLittleEndian(&file, source.width, 4);
    WriteLittleEndian(&file, source.height, 4);
    WriteLittleEndian(&file, 1, 2);
    WriteLittleEndian(&file, 24, 2);
    WriteLittleEndian(&file, 0, 4);
    WriteLittleEndian(&file, image_bytes, 4);
    WriteLittleEndian(&file, 2835, 4);
    WriteLittleEndian(&file, 2835, 4);
    WriteLittleEndian(&file, 0, 4);
    WriteLittleEndian(&file, 0, 4);
    for (uint32_t y = 0; y < source.height; ++y)
    {
        const uint8_t* row = rgba.data() + static_cast<size_t>(y) * source.width * 4;
        for (uint32_t x = 0; x < source.width; ++x)
        {
            file.push_back(row[x * 4 + 2]);
            file.push_back(row[x * 4 + 1]);
            file.push_back(row[x * 4 + 0]);
        }
        for (uint32_t pad = source.width * 3; pad < row_bytes; ++pad)
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
