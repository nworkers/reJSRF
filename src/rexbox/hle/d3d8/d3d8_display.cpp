#include "rexbox/hle/d3d8/d3d8_display.h"

#include "d3d8_display_state.h"
#include "d3d8_gl.h"
#include "d3d8_gl_targets.h"
#include "d3d8_gl_textures.h"
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

VideoMemoryInfo GetVideoMemoryInfo()
{
    VideoMemoryInfo info;
    if (!State().display_created)
    {
        return info;
    }
    const RenderTargetUse targets = RenderTargetBytes();
    info.render_targets = targets.count;
    info.render_target_bytes = targets.bytes;
    const TextureCacheUse textures = TextureCacheBytes();
    info.textures = textures.count;
    info.texture_bytes = textures.bytes;
    // Vendor extensions; an unknown enum only raises GL_INVALID_ENUM, which
    // is read back here so it reaches no one else.
    GlFunctions& gl = Gl();
    const std::string vendor = GetRendererInfo().vendor;
    while (gl.GetError() != 0)
    {
    }
    if (vendor.find("NVIDIA") != std::string::npos)
    {
        GLint total = 0;
        GLint available = 0;
        gl.GetIntegerv(kGlGpuMemoryDedicatedNvx, &total);
        gl.GetIntegerv(kGlGpuMemoryAvailableNvx, &available);
        if (gl.GetError() == 0 && total > 0)
        {
            info.driver_total_known = true;
            info.driver_free_known = true;
            info.driver_total = static_cast<uint64_t>(total) * 1024;
            info.driver_free = static_cast<uint64_t>(available) * 1024;
        }
    }
    else if (vendor.find("ATI") != std::string::npos || vendor.find("AMD") != std::string::npos)
    {
        GLint free_memory[4] = {};
        gl.GetIntegerv(kGlTextureFreeMemoryAti, free_memory);
        if (gl.GetError() == 0 && free_memory[0] > 0)
        {
            info.driver_free_known = true;
            info.driver_free = static_cast<uint64_t>(free_memory[0]) * 1024;
        }
    }
    while (gl.GetError() != 0)
    {
    }
    return info;
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
