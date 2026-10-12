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

}  // namespace

bool LoadGlFunctions(GlFunctions* functions, std::string* missing)
{
    GlFunctions loaded;
    const bool ok = Load(&loaded.GetString, "glGetString", missing) &&
                    Load(&loaded.GetIntegerv, "glGetIntegerv", missing) &&
                    Load(&loaded.Viewport, "glViewport", missing) &&
                    Load(&loaded.ClearColor, "glClearColor", missing) && Load(&loaded.Clear, "glClear", missing);
    if (ok)
    {
        *functions = loaded;
        g_gl = loaded;
    }
    return ok;
}

GlFunctions& Gl()
{
    return g_gl;
}

}  // namespace rexbox::hle::d3d8
