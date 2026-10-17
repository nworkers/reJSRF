#include "d3d8_gl.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

GlFunctions g_gl;
bool g_s3tc = false;  // GL_EXT_texture_compression_s3tc usable
int g_max_samples = 0;     // GL_MAX_SAMPLES, 0 without multisample renderbuffers
int g_max_anisotropy = 1;  // GL_MAX_TEXTURE_MAX_ANISOTROPY, 1 without the extension
bool g_es = false;
uint32_t g_state_epoch = 1;

// REXBOX_GL_CALLS=1: every entry point goes through a wrapper that counts
// its calls (diagnostic, read by d3d8_profile). Each Load line instantiates
// its own wrapper, so each keeps its own pointer to the driver's function.
struct CallCounter
{
    const char* name;
    uint64_t count;
};
std::vector<CallCounter> g_counters;

template <int Id, typename Function>
struct Counted;

template <int Id, typename R, typename... Args>
struct Counted<Id, R(REXBOX_GL_APIENTRY*)(Args...)>
{
    static inline R(REXBOX_GL_APIENTRY* real)(Args...) = nullptr;
    static inline size_t index = 0;
    static R REXBOX_GL_APIENTRY Call(Args... args)
    {
        ++g_counters[index].count;
        return real(args...);
    }
};

template <int Id, typename Function>
void Install(Function* slot, void* address, const char* name)
{
    *slot = reinterpret_cast<Function>(address);
    if (address != nullptr && GlCallCountingEnabled())
    {
        Counted<Id, Function>::real = *slot;
        Counted<Id, Function>::index = g_counters.size();
        g_counters.push_back({name, 0});
        *slot = &Counted<Id, Function>::Call;
    }
}

template <int Id, typename Function>
bool Load(Function* slot, const char* name, std::string* missing)
{
    void* address = platform::GetGlProcAddress(name);
    if (address == nullptr)
    {
        if (missing != nullptr && missing->empty())
        {
            *missing = name;
        }
        return false;
    }
    Install<Id>(slot, address, name);
    return true;
}

template <int Id, typename Function>
void LoadOptional(Function* slot, const char* name)
{
    Install<Id>(slot, platform::GetGlProcAddress(name), name);
}

}  // namespace

bool LoadGlFunctions(GlFunctions* functions, std::string* missing)
{
    GlFunctions f;
    std::string first_missing;
    bool ok = true;
    // Every entry point is tried so the first missing one is reported.
    const auto need = [&ok, &first_missing](bool loaded) { ok = ok && loaded; };
    need(Load<__LINE__>(&f.GetString, "glGetString", &first_missing));
    need(Load<__LINE__>(&f.GetIntegerv, "glGetIntegerv", &first_missing));
    need(Load<__LINE__>(&f.GetError, "glGetError", &first_missing));
    need(Load<__LINE__>(&f.Viewport, "glViewport", &first_missing));
    need(Load<__LINE__>(&f.Scissor, "glScissor", &first_missing));
    need(Load<__LINE__>(&f.Enable, "glEnable", &first_missing));
    need(Load<__LINE__>(&f.Disable, "glDisable", &first_missing));
    need(Load<__LINE__>(&f.ClearColor, "glClearColor", &first_missing));
    need(Load<__LINE__>(&f.ClearStencil, "glClearStencil", &first_missing));
    need(Load<__LINE__>(&f.Clear, "glClear", &first_missing));
    need(Load<__LINE__>(&f.DepthMask, "glDepthMask", &first_missing));
    need(Load<__LINE__>(&f.DepthFunc, "glDepthFunc", &first_missing));
    need(Load<__LINE__>(&f.ColorMask, "glColorMask", &first_missing));
    need(Load<__LINE__>(&f.StencilMask, "glStencilMask", &first_missing));
    need(Load<__LINE__>(&f.StencilFunc, "glStencilFunc", &first_missing));
    need(Load<__LINE__>(&f.StencilOp, "glStencilOp", &first_missing));
    need(Load<__LINE__>(&f.BlendFunc, "glBlendFunc", &first_missing));
    need(Load<__LINE__>(&f.BlendEquation, "glBlendEquation", &first_missing));
    need(Load<__LINE__>(&f.BlendColor, "glBlendColor", &first_missing));
    need(Load<__LINE__>(&f.CullFace, "glCullFace", &first_missing));
    need(Load<__LINE__>(&f.FrontFace, "glFrontFace", &first_missing));
    need(Load<__LINE__>(&f.PolygonOffset, "glPolygonOffset", &first_missing));
    need(Load<__LINE__>(&f.PixelStorei, "glPixelStorei", &first_missing));
    need(Load<__LINE__>(&f.ReadPixels, "glReadPixels", &first_missing));
    need(Load<__LINE__>(&f.CreateShader, "glCreateShader", &first_missing));
    need(Load<__LINE__>(&f.ShaderSource, "glShaderSource", &first_missing));
    need(Load<__LINE__>(&f.CompileShader, "glCompileShader", &first_missing));
    need(Load<__LINE__>(&f.GetShaderiv, "glGetShaderiv", &first_missing));
    need(Load<__LINE__>(&f.GetShaderInfoLog, "glGetShaderInfoLog", &first_missing));
    need(Load<__LINE__>(&f.DeleteShader, "glDeleteShader", &first_missing));
    need(Load<__LINE__>(&f.CreateProgram, "glCreateProgram", &first_missing));
    need(Load<__LINE__>(&f.AttachShader, "glAttachShader", &first_missing));
    need(Load<__LINE__>(&f.LinkProgram, "glLinkProgram", &first_missing));
    need(Load<__LINE__>(&f.GetProgramiv, "glGetProgramiv", &first_missing));
    need(Load<__LINE__>(&f.GetProgramInfoLog, "glGetProgramInfoLog", &first_missing));
    need(Load<__LINE__>(&f.UseProgram, "glUseProgram", &first_missing));
    need(Load<__LINE__>(&f.DeleteProgram, "glDeleteProgram", &first_missing));
    LoadOptional<__LINE__>(&f.GetProgramBinary, "glGetProgramBinary");
    LoadOptional<__LINE__>(&f.ProgramBinary, "glProgramBinary");
    LoadOptional<__LINE__>(&f.ProgramParameteri, "glProgramParameteri");
    need(Load<__LINE__>(&f.GetUniformLocation, "glGetUniformLocation", &first_missing));
    need(Load<__LINE__>(&f.Uniform1i, "glUniform1i", &first_missing));
    need(Load<__LINE__>(&f.Uniform1f, "glUniform1f", &first_missing));
    need(Load<__LINE__>(&f.Uniform2f, "glUniform2f", &first_missing));
    need(Load<__LINE__>(&f.Uniform4f, "glUniform4f", &first_missing));
    need(Load<__LINE__>(&f.Uniform4fv, "glUniform4fv", &first_missing));
    need(Load<__LINE__>(&f.UniformMatrix4fv, "glUniformMatrix4fv", &first_missing));
    need(Load<__LINE__>(&f.GenBuffers, "glGenBuffers", &first_missing));
    need(Load<__LINE__>(&f.DeleteBuffers, "glDeleteBuffers", &first_missing));
    need(Load<__LINE__>(&f.BindBuffer, "glBindBuffer", &first_missing));
    need(Load<__LINE__>(&f.BufferData, "glBufferData", &first_missing));
    need(Load<__LINE__>(&f.BufferSubData, "glBufferSubData", &first_missing));
    need(Load<__LINE__>(&f.GenVertexArrays, "glGenVertexArrays", &first_missing));
    need(Load<__LINE__>(&f.BindVertexArray, "glBindVertexArray", &first_missing));
    need(Load<__LINE__>(&f.EnableVertexAttribArray, "glEnableVertexAttribArray", &first_missing));
    need(Load<__LINE__>(&f.DisableVertexAttribArray, "glDisableVertexAttribArray", &first_missing));
    need(Load<__LINE__>(&f.VertexAttribPointer, "glVertexAttribPointer", &first_missing));
    need(Load<__LINE__>(&f.DrawArrays, "glDrawArrays", &first_missing));
    need(Load<__LINE__>(&f.DrawElements, "glDrawElements", &first_missing));
    need(Load<__LINE__>(&f.GenTextures, "glGenTextures", &first_missing));
    need(Load<__LINE__>(&f.DeleteTextures, "glDeleteTextures", &first_missing));
    need(Load<__LINE__>(&f.BindTexture, "glBindTexture", &first_missing));
    need(Load<__LINE__>(&f.ActiveTexture, "glActiveTexture", &first_missing));
    need(Load<__LINE__>(&f.TexImage2D, "glTexImage2D", &first_missing));
    need(Load<__LINE__>(&f.TexImage3D, "glTexImage3D", &first_missing));
    need(Load<__LINE__>(&f.TexParameteri, "glTexParameteri", &first_missing));
    need(Load<__LINE__>(&f.GenSamplers, "glGenSamplers", &first_missing));
    need(Load<__LINE__>(&f.BindSampler, "glBindSampler", &first_missing));
    need(Load<__LINE__>(&f.SamplerParameteri, "glSamplerParameteri", &first_missing));
    need(Load<__LINE__>(&f.SamplerParameterf, "glSamplerParameterf", &first_missing));
    need(Load<__LINE__>(&f.GenFramebuffers, "glGenFramebuffers", &first_missing));
    need(Load<__LINE__>(&f.DeleteFramebuffers, "glDeleteFramebuffers", &first_missing));
    need(Load<__LINE__>(&f.BindFramebuffer, "glBindFramebuffer", &first_missing));
    need(Load<__LINE__>(&f.FramebufferTexture2D, "glFramebufferTexture2D", &first_missing));
    need(Load<__LINE__>(&f.FramebufferRenderbuffer, "glFramebufferRenderbuffer", &first_missing));
    need(Load<__LINE__>(&f.CheckFramebufferStatus, "glCheckFramebufferStatus", &first_missing));
    need(Load<__LINE__>(&f.GenRenderbuffers, "glGenRenderbuffers", &first_missing));
    need(Load<__LINE__>(&f.DeleteRenderbuffers, "glDeleteRenderbuffers", &first_missing));
    need(Load<__LINE__>(&f.BindRenderbuffer, "glBindRenderbuffer", &first_missing));
    need(Load<__LINE__>(&f.RenderbufferStorage, "glRenderbufferStorage", &first_missing));
    need(Load<__LINE__>(&f.BlitFramebuffer, "glBlitFramebuffer", &first_missing));
    LoadOptional<__LINE__>(&f.ClearDepthf, "glClearDepthf");
    LoadOptional<__LINE__>(&f.ClearDepth, "glClearDepth");
    LoadOptional<__LINE__>(&f.DepthRangef, "glDepthRangef");
    LoadOptional<__LINE__>(&f.DepthRange, "glDepthRange");
    LoadOptional<__LINE__>(&f.PolygonMode, "glPolygonMode");
    LoadOptional<__LINE__>(&f.CompressedTexImage2D, "glCompressedTexImage2D");
    LoadOptional<__LINE__>(&f.GetStringi, "glGetStringi");
    LoadOptional<__LINE__>(&f.RenderbufferStorageMultisample, "glRenderbufferStorageMultisample");
    if (ok && f.ClearDepthf == nullptr && f.ClearDepth == nullptr)
    {
        first_missing = "glClearDepth";
        ok = false;
    }
    if (ok && f.DepthRangef == nullptr && f.DepthRange == nullptr)
    {
        first_missing = "glDepthRange";
        ok = false;
    }
    if (!ok)
    {
        if (missing != nullptr)
        {
            *missing = first_missing;
        }
        return false;
    }
    *functions = f;
    g_gl = f;
    // The extension list (core profiles list them one by one).
    g_s3tc = false;
    bool anisotropic = false;
    if (f.GetStringi != nullptr)
    {
        GLint count = 0;
        f.GetIntegerv(kGlNumExtensions, &count);
        for (GLint i = 0; i < count; ++i)
        {
            const auto* name = reinterpret_cast<const char*>(f.GetStringi(kGlExtensions, static_cast<GLuint>(i)));
            if (name == nullptr)
            {
                continue;
            }
            g_s3tc = g_s3tc || (f.CompressedTexImage2D != nullptr &&
                                std::strcmp(name, "GL_EXT_texture_compression_s3tc") == 0);
            anisotropic = anisotropic || std::strcmp(name, "GL_EXT_texture_filter_anisotropic") == 0 ||
                          std::strcmp(name, "GL_ARB_texture_filter_anisotropic") == 0;
        }
    }
    g_max_samples = 0;
    if (f.RenderbufferStorageMultisample != nullptr)
    {
        GLint samples = 0;
        f.GetIntegerv(kGlMaxSamples, &samples);
        g_max_samples = samples > 1 ? samples : 0;
    }
    g_max_anisotropy = 1;
    if (anisotropic && f.SamplerParameterf != nullptr)
    {
        GLint anisotropy = 1;
        f.GetIntegerv(kGlMaxTextureMaxAnisotropy, &anisotropy);
        g_max_anisotropy = anisotropy > 1 ? anisotropy : 1;
    }
    while (f.GetError() != 0)
    {
    }
    return true;
}

int GlMaxSamples()
{
    return g_max_samples;
}

int GlMaxAnisotropy()
{
    return g_max_anisotropy;
}

bool GlHasS3tc()
{
    return g_s3tc;
}

GlFunctions& Gl()
{
    return g_gl;
}

bool GlIsEs()
{
    return g_es;
}

void SetGlIsEs(bool es)
{
    g_es = es;
}

void GlClearDepth(float depth)
{
    if (g_gl.ClearDepthf != nullptr)
    {
        g_gl.ClearDepthf(depth);
    }
    else
    {
        g_gl.ClearDepth(static_cast<GLdouble>(depth));
    }
}

bool GlCallCountingEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("REXBOX_GL_CALLS");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }();
    return enabled;
}

std::vector<std::pair<const char*, uint64_t>> TakeGlCallCounts()
{
    std::vector<std::pair<const char*, uint64_t>> counts;
    for (CallCounter& counter : g_counters)
    {
        if (counter.count != 0)
        {
            counts.emplace_back(counter.name, counter.count);
            counter.count = 0;
        }
    }
    std::sort(counts.begin(), counts.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return counts;
}

void InvalidateGlStateCache()
{
    ++g_state_epoch;
}

uint32_t GlStateEpoch()
{
    return g_state_epoch;
}

void GlDepthRange(float near_value, float far_value)
{
    if (g_gl.DepthRangef != nullptr)
    {
        g_gl.DepthRangef(near_value, far_value);
    }
    else
    {
        g_gl.DepthRange(static_cast<GLdouble>(near_value), static_cast<GLdouble>(far_value));
    }
}

}  // namespace rexbox::hle::d3d8
