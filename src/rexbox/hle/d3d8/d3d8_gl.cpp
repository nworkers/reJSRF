#include "d3d8_gl.h"

namespace rexbox::hle::d3d8
{

namespace
{

GlFunctions g_gl;

template <typename Function>
bool Load(Function* slot, const char* name, std::string* missing)
{
    void* address = platform::GetGlProcAddress(name);
    if (address == nullptr)
    {
        if (missing != nullptr)
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
    GlFunctions loaded;
    const bool ok = Load(&loaded.GetString, "glGetString", missing) &&
                    Load(&loaded.GetIntegerv, "glGetIntegerv", missing) &&
                    Load(&loaded.Viewport, "glViewport", missing) && Load(&loaded.Scissor, "glScissor", missing) &&
                    Load(&loaded.Enable, "glEnable", missing) && Load(&loaded.Disable, "glDisable", missing) &&
                    Load(&loaded.ClearColor, "glClearColor", missing) &&
                    Load(&loaded.ClearStencil, "glClearStencil", missing) && Load(&loaded.Clear, "glClear", missing) &&
                    Load(&loaded.DepthMask, "glDepthMask", missing) && Load(&loaded.ColorMask, "glColorMask", missing) &&
                    Load(&loaded.StencilMask, "glStencilMask", missing);
    if (!ok)
    {
        return false;
    }
    LoadOptional(&loaded.ClearDepthf, "glClearDepthf");
    LoadOptional(&loaded.ClearDepth, "glClearDepth");
    LoadOptional(&loaded.DepthRangef, "glDepthRangef");
    LoadOptional(&loaded.DepthRange, "glDepthRange");
    if (loaded.ClearDepthf == nullptr && loaded.ClearDepth == nullptr)
    {
        *missing = "glClearDepth";
        return false;
    }
    if (loaded.DepthRangef == nullptr && loaded.DepthRange == nullptr)
    {
        *missing = "glDepthRange";
        return false;
    }
    *functions = loaded;
    g_gl = loaded;
    return true;
}

GlFunctions& Gl()
{
    return g_gl;
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
