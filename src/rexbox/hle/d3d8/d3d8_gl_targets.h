// Render targets on GL: each color surface the title draws to is a GL
// texture, each depth surface a depth/stencil renderbuffer, and every pair in
// use gets a framebuffer object. The window shows a back buffer only when
// Swap blits it (docs/design/20261012-i023-d3d8-fixed-function-draw.md
// section 3.5). GL's bottom-left origin is kept inside the targets; the
// drawing path flips D3D's top-left coordinates once, in the vertex shader
// and the viewport/scissor conversion.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_TARGETS_H
#define REXBOX_HLE_D3D8_D3D8_GL_TARGETS_H

#include "d3d8_gamma.h"
#include "rexbox/hle/d3d8/d3d8_display.h"

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rexbox::hle::d3d8
{

using runtime::GuestAddress;

// Binds the framebuffer for the color surface and depth surface (either may
// be 0) and makes later draws and clears go there. Returns false when GL
// could not build it (the reason is reported once).
bool BindRenderTarget(GuestAddress color, GuestAddress depth);

// Size of the color target last bound (or the depth target without color),
// in guest pixels: what D3D sees whatever the render scale.
uint32_t RenderTargetWidth();
uint32_t RenderTargetHeight();

// The internal rendering scale (1..8): render targets are drawn at this many
// times their guest size, so GL viewports and scissors take guest pixels times
// it. REXBOX_RENDER_SCALE sets the first value; SetRenderScale (and Ctrl+1..8
// in the window) asks for another, which takes effect after the next present
// (docs/design/20261017-i080-render-scale.md).
uint32_t RenderScale();
void SetRenderScale(uint32_t scale);

// Hor+ widescreen (docs/design/20261018-i085-widescreen-horplus.md). Back
// buffers and depth stencils are stored WideWidth(aspect, height) guest
// pixels wide (0: their own width) and draws to them scale clip x by
// RenderTargetWideFactor() (width / wide, 1 for other targets), so the 4:3
// picture keeps its place in the middle and the scene continues at the
// sides. SetTargetAspect takes effect after the next present (at once before
// any target exists).
uint32_t WideWidth(AspectMode aspect, uint32_t height);
void SetTargetAspect(AspectMode aspect);
AspectMode TargetAspect();
float RenderTargetWideFactor();

// Hor+ widescreen (docs/design/20261018-i085-widescreen-horplus.md). Back
// buffers and depth stencils are stored WideWidth(aspect, height) guest
// pixels wide (0: their own width) and draws to them scale clip x by
// RenderTargetWideFactor() (width / wide, 1 for other targets), so the 4:3
// picture keeps its place in the middle and the scene continues at the
// sides. SetTargetAspect takes effect after the next present (at once before
// any target exists).
uint32_t WideWidth(AspectMode aspect, uint32_t height);
void SetTargetAspect(AspectMode aspect);
AspectMode TargetAspect();
float RenderTargetWideFactor();

// The GL storage of the render targets (color with its sampling copy, and
// depth/stencil), at their scaled size: how many surfaces and their bytes.
struct RenderTargetUse
{
    uint32_t count = 0;
    uint64_t bytes = 0;
};
RenderTargetUse RenderTargetBytes();

// Shows `color` (a back buffer) in the window through the display gamma ramp,
// scaled to fit with its aspect ratio kept, and rebinds the current render target.
void PresentRenderTarget(GuestAddress color, const GammaRamp& ramp);

// Writes `color`'s pixels, through the gamma ramp as on screen, to a 24-bit
// BMP file. Returns false on failure.
bool DumpRenderTarget(GuestAddress color, const std::string& path, const GammaRamp& ramp);

// A color target's pixels as RGBA8 in GL's row order (bottom row first), at
// its guest size. DumpRenderTarget writes the GL size (guest size times the
// render scale).
void ReadRenderTarget(GuestAddress color, std::vector<uint8_t>* rgba, uint32_t* width, uint32_t* height);

// RGBA8 pixels, bottom row first, as a 24-bit BMP file. False on failure.
bool WriteBmp(const std::string& path, const uint8_t* rgba, uint32_t width, uint32_t height);

// For a texture over the memory of a color surface bound as a render target:
// a GL texture with the target's current pixels in D3D's row order (refreshed
// when the target was bound since), with the header last bound over that
// memory and the size; 0 when no target is at that memory. Targets are keyed
// by memory, not header (docs/design/20261016-i069-render-target-sampling.md,
// docs/design/20261017-i071-render-target-by-memory.md).
uint32_t SampleRenderTarget(GuestAddress data, GuestAddress* surface, uint32_t* width, uint32_t* height);

// Pixel memory at `data` was freed: textures there no longer sample a target.
void ForgetRenderTargetMemory(GuestAddress data);

// The GL texture behind a color surface used as a render target, or 0.
uint32_t RenderTargetTexture(GuestAddress color);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_TARGETS_H
