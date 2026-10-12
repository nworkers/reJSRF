#include "d3d8_gl.h"

namespace rexbox::hle::d3d8
{

namespace
{

GlFunctions g_gl;
bool g_es = false;

template <typename Function>
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
    *slot = reinterpret_cast<Function>(address);
    return true;
}

template <typename Function>
void LoadOptional(Function* slot, const char* name)
{
    void* address = platform::GetGlProcAddress(name);
    *slot = address != nullptr ? reinterpret_cast<Function>(address) : nullptr;
}

}  // namespace

bool LoadGlFunctions(GlFunctions* functions, std::string* missing)
{
    GlFunctions f;
    std::string first_missing;
    bool ok = true;
    // Every entry point is tried so the first missing one is reported.
    const auto need = [&ok, &first_missing](bool loaded) { ok = ok && loaded; };
    need(Load(&f.GetString, "glGetString", &first_missing));
    need(Load(&f.GetIntegerv, "glGetIntegerv", &first_missing));
    need(Load(&f.GetError, "glGetError", &first_missing));
    need(Load(&f.Viewport, "glViewport", &first_missing));
    need(Load(&f.Scissor, "glScissor", &first_missing));
    need(Load(&f.Enable, "glEnable", &first_missing));
    need(Load(&f.Disable, "glDisable", &first_missing));
    need(Load(&f.ClearColor, "glClearColor", &first_missing));
    need(Load(&f.ClearStencil, "glClearStencil", &first_missing));
    need(Load(&f.Clear, "glClear", &first_missing));
    need(Load(&f.DepthMask, "glDepthMask", &first_missing));
    need(Load(&f.DepthFunc, "glDepthFunc", &first_missing));
    need(Load(&f.ColorMask, "glColorMask", &first_missing));
    need(Load(&f.StencilMask, "glStencilMask", &first_missing));
    need(Load(&f.StencilFunc, "glStencilFunc", &first_missing));
    need(Load(&f.StencilOp, "glStencilOp", &first_missing));
    need(Load(&f.BlendFunc, "glBlendFunc", &first_missing));
    need(Load(&f.BlendEquation, "glBlendEquation", &first_missing));
    need(Load(&f.BlendColor, "glBlendColor", &first_missing));
    need(Load(&f.CullFace, "glCullFace", &first_missing));
    need(Load(&f.FrontFace, "glFrontFace", &first_missing));
    need(Load(&f.PolygonOffset, "glPolygonOffset", &first_missing));
    need(Load(&f.PixelStorei, "glPixelStorei", &first_missing));
    need(Load(&f.ReadPixels, "glReadPixels", &first_missing));
    need(Load(&f.CreateShader, "glCreateShader", &first_missing));
    need(Load(&f.ShaderSource, "glShaderSource", &first_missing));
    need(Load(&f.CompileShader, "glCompileShader", &first_missing));
    need(Load(&f.GetShaderiv, "glGetShaderiv", &first_missing));
    need(Load(&f.GetShaderInfoLog, "glGetShaderInfoLog", &first_missing));
    need(Load(&f.DeleteShader, "glDeleteShader", &first_missing));
    need(Load(&f.CreateProgram, "glCreateProgram", &first_missing));
    need(Load(&f.AttachShader, "glAttachShader", &first_missing));
    need(Load(&f.LinkProgram, "glLinkProgram", &first_missing));
    need(Load(&f.GetProgramiv, "glGetProgramiv", &first_missing));
    need(Load(&f.GetProgramInfoLog, "glGetProgramInfoLog", &first_missing));
    need(Load(&f.UseProgram, "glUseProgram", &first_missing));
    need(Load(&f.GetUniformLocation, "glGetUniformLocation", &first_missing));
    need(Load(&f.Uniform1i, "glUniform1i", &first_missing));
    need(Load(&f.Uniform1f, "glUniform1f", &first_missing));
    need(Load(&f.Uniform2f, "glUniform2f", &first_missing));
    need(Load(&f.Uniform4f, "glUniform4f", &first_missing));
    need(Load(&f.UniformMatrix4fv, "glUniformMatrix4fv", &first_missing));
    need(Load(&f.GenBuffers, "glGenBuffers", &first_missing));
    need(Load(&f.BindBuffer, "glBindBuffer", &first_missing));
    need(Load(&f.BufferData, "glBufferData", &first_missing));
    need(Load(&f.GenVertexArrays, "glGenVertexArrays", &first_missing));
    need(Load(&f.BindVertexArray, "glBindVertexArray", &first_missing));
    need(Load(&f.EnableVertexAttribArray, "glEnableVertexAttribArray", &first_missing));
    need(Load(&f.DisableVertexAttribArray, "glDisableVertexAttribArray", &first_missing));
    need(Load(&f.VertexAttribPointer, "glVertexAttribPointer", &first_missing));
    need(Load(&f.DrawArrays, "glDrawArrays", &first_missing));
    need(Load(&f.DrawElements, "glDrawElements", &first_missing));
    need(Load(&f.GenTextures, "glGenTextures", &first_missing));
    need(Load(&f.DeleteTextures, "glDeleteTextures", &first_missing));
    need(Load(&f.BindTexture, "glBindTexture", &first_missing));
    need(Load(&f.ActiveTexture, "glActiveTexture", &first_missing));
    need(Load(&f.TexImage2D, "glTexImage2D", &first_missing));
    need(Load(&f.TexImage3D, "glTexImage3D", &first_missing));
    need(Load(&f.TexParameteri, "glTexParameteri", &first_missing));
    need(Load(&f.GenSamplers, "glGenSamplers", &first_missing));
    need(Load(&f.BindSampler, "glBindSampler", &first_missing));
    need(Load(&f.SamplerParameteri, "glSamplerParameteri", &first_missing));
    need(Load(&f.SamplerParameterf, "glSamplerParameterf", &first_missing));
    need(Load(&f.GenFramebuffers, "glGenFramebuffers", &first_missing));
    need(Load(&f.BindFramebuffer, "glBindFramebuffer", &first_missing));
    need(Load(&f.FramebufferTexture2D, "glFramebufferTexture2D", &first_missing));
    need(Load(&f.FramebufferRenderbuffer, "glFramebufferRenderbuffer", &first_missing));
    need(Load(&f.CheckFramebufferStatus, "glCheckFramebufferStatus", &first_missing));
    need(Load(&f.GenRenderbuffers, "glGenRenderbuffers", &first_missing));
    need(Load(&f.BindRenderbuffer, "glBindRenderbuffer", &first_missing));
    need(Load(&f.RenderbufferStorage, "glRenderbufferStorage", &first_missing));
    need(Load(&f.BlitFramebuffer, "glBlitFramebuffer", &first_missing));
    LoadOptional(&f.ClearDepthf, "glClearDepthf");
    LoadOptional(&f.ClearDepth, "glClearDepth");
    LoadOptional(&f.DepthRangef, "glDepthRangef");
    LoadOptional(&f.DepthRange, "glDepthRange");
    LoadOptional(&f.PolygonMode, "glPolygonMode");
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
    return true;
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
