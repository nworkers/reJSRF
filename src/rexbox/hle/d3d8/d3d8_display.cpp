#include "rexbox/hle/d3d8/d3d8_display.h"

#include "d3d8_display_state.h"
#include "d3d8_gl.h"
#include "d3d8_internal.h"

#include <atomic>
#include <mutex>

namespace rexbox::hle::d3d8
{

namespace
{

std::mutex g_mutex;
DisplayDefaults g_defaults;
RendererInfo g_renderer;
std::atomic<bool> g_widescreen{false};

std::string GlString(GlFunctions& gl, GLenum name)
{
    const auto* text = reinterpret_cast<const char*>(gl.GetString(name));
    return text != nullptr ? std::string(text) : std::string();
}

}  // namespace

void SetDisplayDefaults(const DisplayDefaults& defaults)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_defaults = defaults;
    g_widescreen.store(defaults.widescreen);
}

DisplayDefaults CurrentDisplayDefaults()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_defaults;
}

void RecordRendererInfo(GlFunctions& gl)
{
    RendererInfo info;
    info.vendor = GlString(gl, kGlVendor);
    info.renderer = GlString(gl, kGlRenderer);
    info.version = GlString(gl, kGlVersion);
    info.shading_language = GlString(gl, kGlShadingLanguageVersion);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_renderer = std::move(info);
}

RendererInfo GetRendererInfo()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_renderer;
}

bool Widescreen()
{
    return g_widescreen.load();
}

void SetWidescreen(bool widescreen)
{
    g_widescreen.store(widescreen);
    ApplyPictureAspect();
}

void ApplyPictureAspect()
{
    const PresentParameters& pp = State().present;
    if (!State().display_created || pp.back_buffer_width == 0 || pp.back_buffer_height == 0)
    {
        return;
    }
    const uint32_t width = g_widescreen.load() ? (pp.back_buffer_height * 16 + 8) / 9 : pp.back_buffer_width;
    platform::SetPictureAspect(width, pp.back_buffer_height);
}

}  // namespace rexbox::hle::d3d8
