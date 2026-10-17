// HLE module D3D8: how the picture is shown, for the application's settings
// (docs/design/20261017-i084-settings-overlay.md). What the title sees does
// not change.

#ifndef REXBOX_HLE_D3D8_D3D8_DISPLAY_H
#define REXBOX_HLE_D3D8_D3D8_DISPLAY_H

#include "rexbox/platform/display.h"

#include <cstdint>
#include <string>

namespace rexbox::hle::d3d8
{

struct DisplayDefaults
{
    uint32_t render_scale = 1;  // 1..8 (docs/design/20261017-i080-render-scale.md)
    bool widescreen = false;    // 16:9, stretched for now (issue #85)
    platform::VsyncMode vsync = platform::VsyncMode::kAdaptive;
    bool fullscreen = false;
};

// The settings to start with; call before the title creates its device.
// REXBOX_RENDER_SCALE and REXBOX_VSYNC win for the run when set.
void SetDisplayDefaults(const DisplayDefaults& defaults);

// The internal rendering scale (1..8); a change takes effect after the next
// present.
uint32_t RenderScale();
void SetRenderScale(uint32_t scale);

// Shows the picture at 16:9 instead of 4:3 (stretched). Call on the thread
// that presents (the overlay's builder runs there).
bool Widescreen();
void SetWidescreen(bool widescreen);

// The GL driver behind the device; empty before it exists.
struct RendererInfo
{
    std::string vendor;
    std::string renderer;
    std::string version;
    std::string shading_language;
};
RendererInfo GetRendererInfo();

// Video memory: what the driver reports (NVIDIA and AMD extensions; not
// every driver tells) and what this HLE allocated. Call on the thread that
// presents (it queries GL).
struct VideoMemoryInfo
{
    bool driver_total_known = false;
    bool driver_free_known = false;
    uint64_t driver_total = 0;  // bytes of dedicated video memory
    uint64_t driver_free = 0;   // bytes available now
    uint32_t render_targets = 0;
    uint64_t render_target_bytes = 0;
    uint32_t textures = 0;
    uint64_t texture_bytes = 0;
};
VideoMemoryInfo GetVideoMemoryInfo();

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_DISPLAY_H
