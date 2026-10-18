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

// SMAA modes (docs/design/20261017-i089-anti-aliasing.md).
enum class SmaaMode
{
    kOff,
    k1x,
};

// The picture's aspect (docs/design/20261018-i085-widescreen-horplus.md):
// 16:9 and 21:9 widen the back buffer and the horizontal field of view
// (Hor+); screen-space drawing stays centered at its 4:3 size.
enum class AspectMode
{
    k4x3,
    k16x9,
    k21x9,
};

struct DisplayDefaults
{
    uint32_t render_scale = 1;  // 1..8 (docs/design/20261017-i080-render-scale.md)
    AspectMode aspect = AspectMode::k4x3;
    platform::VsyncMode vsync = platform::VsyncMode::kAdaptive;
    bool fullscreen = false;
    bool vblank_wait = true;    // false: nothing waits for the virtual vertical blank (issue #95)
    uint32_t window_scale = 2;  // 1..3, the window's size at start (docs/design/20261017-i090-window-size.md)
    uint32_t msaa_samples = 0;  // 0 (off), 2, 4, 8, 16
    SmaaMode smaa = SmaaMode::kOff;
    uint32_t anisotropy = 1;  // 1 (off), 2, 4, 8, 16
};

// The settings to start with; call before the title creates its device.
// REXBOX_RENDER_SCALE and REXBOX_VSYNC win for the run when set.
void SetDisplayDefaults(const DisplayDefaults& defaults);

// The internal rendering scale (1..8); a change takes effect after the next
// present.
uint32_t RenderScale();
void SetRenderScale(uint32_t scale);

// Anti-aliasing and texture filtering (docs/design/20261017-i089-anti-aliasing.md).
// MSAA: a change takes effect after the next present; values are rounded
// down to a power of two the driver supports (0 when it has none). The
// limits are known once the device exists.
uint32_t MsaaSamples();
void SetMsaaSamples(uint32_t samples);
uint32_t MaxMsaaSamples();
// SMAA on the presented picture.
SmaaMode Smaa();
void SetSmaa(SmaaMode mode);
// Anisotropic filtering on linear-filtered textures (1: off), at most
// MaxAnisotropy().
uint32_t Anisotropy();
void SetAnisotropy(uint32_t anisotropy);
uint32_t MaxAnisotropy();

// Shows the picture at 16:9 instead of 4:3 (stretched). Call on the thread
// that presents (the overlay's builder runs there).
// The picture's aspect; a change takes effect after the next present.
AspectMode Aspect();
void SetAspect(AspectMode aspect);

// Whether the title waits for the virtual vertical blank
// (docs/design/20261018-i095-vblank-wait.md). Off, it runs as fast as the
// host draws it, and game speed follows the frame rate. Takes effect at once.
bool VerticalBlankWaitEnabled();
void SetVerticalBlankWaitEnabled(bool wait);

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

// How much wider the picture's horizontal view is than the title's 4:3 one
// (docs/design/20261018-i097-frustum-culling.md): the widened back buffer's
// width over 640, or 1. C linkage so decompiled title code (C) can declare it
// without this header, letting a title's own culling admit what Hor+ shows.
extern "C" float rexbox_d3d8_horizontal_view_scale(void);

#endif  // REXBOX_HLE_D3D8_D3D8_DISPLAY_H
